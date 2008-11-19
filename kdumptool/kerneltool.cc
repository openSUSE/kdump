/*
 * (c) 2008, Bernhard Walle <bwalle@suse.de>, SUSE LINUX Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include <string>
#include <cerrno>
#include <cstring>
#include <memory>
#include <sstream>
#include <list>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <zlib.h>
#include <libelf.h>
#include <gelf.h>

#include "kerneltool.h"
#include "util.h"
#include "global.h"
#include "debug.h"
#include "stringutil.h"
#include "fileutil.h"
#include "kconfig.h"

using std::string;
using std::memset;
using std::auto_ptr;
using std::stringstream;
using std::list;

/* x86 boot header for bzImage */
#define X86_HEADER_OFF_START        0x202
#define X86_HEADER_OFF_VERSION      0x0206
#define X86_HEADER_OFF_RELOCATABLE  0x0234
#define X86_HEADER_OFF_MAGIC        0x53726448
#define X86_HEADER_RELOCATABLE_VER  0x0205

#define MAGIC_START         "IKCFG_ST"
#define MAGIC_END           "IKCFG_ED"
#define MAGIC_LEN           8


// -----------------------------------------------------------------------------
KernelTool::KernelTool(const std::string &image)
    throw (KError)
    : m_kernel(image), m_fd(-1)
{
    Debug::debug()->trace("KernelTool::KernelTool(%s)", image.c_str());

    m_fd = open(image.c_str(), O_RDONLY);
    if (m_fd < 0) {
        throw KSystemError("Opening of " + image + " failed.", errno);
    }
}

// -----------------------------------------------------------------------------
KernelTool::~KernelTool()
{
    close(m_fd);
    m_fd = -1;
}


// -----------------------------------------------------------------------------
list<string> KernelTool::imageNames(const std::string &arch)
    throw ()
{
    list<string> ret;

    if (arch == "i386" || arch == "x86_64") {
        ret.push_back("vmlinuz");
        ret.push_back("vmlinux");
    } else if (arch == "ia64") {
        ret.push_back("vmlinuz");
    } else {
        ret.push_back("vmlinux");
    }

    return ret;
}

// -----------------------------------------------------------------------------
bool KernelTool::stripImageName(const string &kernelImage, string &directory,
                                string &rest)
    throw (KError)
{
    directory = FileUtil::dirname(kernelImage);
    string kernel = FileUtil::baseName(kernelImage);

    list<string> imageNames = KernelTool::imageNames(Util::getArch());
    for (list<string>::const_iterator it = imageNames.begin();
            it != imageNames.end(); ++it) {
        if (kernel == *it) {
            rest = "";
            return true;
        }
        rest = Stringutil::stripPrefix(kernel, *it + "-");
        if (rest != kernel) {
            return true;
        }
        
    }

    return false;
}

// -----------------------------------------------------------------------------
KernelTool::KernelType KernelTool::getKernelType() const
    throw (KError)
{
    if (Util::isElfFile(m_fd)) {
        if (Util::isGzipFile(m_fd))
            return KT_ELF_GZ;
        else
            return KT_ELF;
    } else {
        if (!Util::isX86(Util::getArch()))
            return KT_NONE;
        else {
            if (isX86Kernel())
                return KT_X86;
            else
                return KT_NONE;
        }
    }
}

// -----------------------------------------------------------------------------
bool KernelTool::isRelocatable() const
    throw (KError)
{
    switch (getKernelType()) {
        case KernelTool::KT_ELF:
        case KernelTool::KT_ELF_GZ:
            return elfIsRelocatable();

        case KernelTool::KT_X86:
            return x86isRelocatable();
            break;

        default:
            throw KError("Invalid kernel type.");
            break;
    }
}

// -----------------------------------------------------------------------------
bool KernelTool::isX86Kernel() const
    throw (KError)
{
    unsigned char buffer[BUFSIZ];

    /* check the magic number */
    off_t off_ret = lseek(m_fd, X86_HEADER_OFF_START, SEEK_SET);
    if (off_ret == (off_t)-1) {
        throw KSystemError("IdentifyKernel::isX86Kernel: lseek to "
            "X86_HEADER_OFF_START failed", errno);
    }

    int ret = read(m_fd, buffer, 4);
    if (ret != 4) {
        throw KSystemError("IdentifyKernel::isX86Kernel: read of magic "
            "start failed", errno);
    }

    return *((uint32_t *)buffer) == X86_HEADER_OFF_MAGIC;
}

// -----------------------------------------------------------------------------
bool KernelTool::x86isRelocatable() const
    throw (KError)
{
    unsigned char buffer[BUFSIZ];

    // check the magic number
    off_t off_ret = lseek(m_fd, X86_HEADER_OFF_START, SEEK_SET);
    if (off_ret == (off_t)-1) {
        throw KSystemError("IdentifyKernel::checkArchFileX86: lseek to "
            "X86_HEADER_OFF_START failed", errno);
    }

    int ret = read(m_fd, buffer, 4);
    if (ret != 4) {
        throw KSystemError("IdentifyKernel::checkArchFileX86: read of magic "
            "start failed", errno);
    }

    if (*((uint32_t *)buffer) != X86_HEADER_OFF_MAGIC) {
        throw KError("This is not a kernel image");
    }

    // check the version
    off_ret = lseek(m_fd, X86_HEADER_OFF_VERSION, SEEK_SET);
    if (off_ret == (off_t)-1) {
        throw KSystemError("IdentifyKernel::checkArchFileX86: lseek to "
            "X86_HEADER_OFF_VERSION failed", errno);
    }

    ret = read(m_fd, buffer, 2);
    if (ret != 2) {
        throw KSystemError("IdentifyKernel::checkArchFileX86: read of version"
            " failed", errno);
    }

    // older versions are not relocatable
    if (*((uint16_t *)buffer) < X86_HEADER_RELOCATABLE_VER) {
        return false;
    }

    // and check if the kernel is compiled to be relocatable
    off_ret = lseek(m_fd, X86_HEADER_OFF_RELOCATABLE, SEEK_SET);
    if (off_ret == (off_t)-1) {
        throw KSystemError("IdentifyKernel::checkArchFileX86: lseek to "
            "X86_HEADER_OFF_RELOCATABLE failed", errno);
    }

    ret = read(m_fd, buffer, 1);
    if (ret != 1) {
        throw KSystemError("IdentifyKernel::checkArchFileX86: read of "
            "relocatable bit failed", errno);
    }

    return !!buffer[0];
}

// -----------------------------------------------------------------------------
bool KernelTool::elfIsRelocatable() const
    throw (KError)
{
    unsigned char e_ident[EI_NIDENT];

    off_t oret = lseek(m_fd, 0, SEEK_SET);
    if (oret == off_t(-1)) {
        throw KSystemError("lseek() failed", errno);
    }

    gzFile fp = gzdopen(dup(m_fd), "r");
    if (!fp) {
        throw KSystemError("check_elf_file: Failed to open file", errno);
    }

    int ret = gzread(fp, e_ident, EI_NIDENT);
    if (ret != EI_NIDENT) {
        gzclose(fp);
        throw KSystemError("check_elf_file: Failed to read", errno);
    }

    if (gzseek(fp, 0, SEEK_SET) == (off_t)-1) {
        gzclose(fp);
        throw KSystemError("Seek failed", errno);
    }

    string arch;
    bool reloc = false;
    if (e_ident[EI_CLASS] == ELFCLASS32) {
        Elf32_Ehdr hdr;

        ret = gzread(fp, (unsigned char *)&hdr, sizeof(Elf32_Ehdr));
        if (ret != sizeof(Elf32_Ehdr)) {
            gzclose(fp);
            throw KSystemError("Couldn't read ELF header", errno);
        }

        arch = archFromElfMachine((unsigned long long)hdr.e_machine);

        if (hdr.e_type == ET_DYN)
            reloc = true;
    } else if (e_ident[EI_CLASS] == ELFCLASS64) {
        Elf64_Ehdr hdr;

        ret = gzread(fp, (unsigned char *)&hdr, sizeof(Elf64_Ehdr));
        if (ret != sizeof(Elf64_Ehdr)) {
            gzclose(fp);
            throw KSystemError("Couldn't read ELF header", errno);
        }

        if (hdr.e_type == ET_DYN)
            reloc = true;

        arch = archFromElfMachine((unsigned long long)hdr.e_machine);
    } else {
        throw KError("elfIsRelocatable(): Invalid ELF class");
    }

    gzclose(fp);

    Debug::debug()->dbg("Detected arch %s", arch.c_str());

    return isArchAlwaysRelocatable(arch) || reloc;
}

// -----------------------------------------------------------------------------
bool KernelTool::isArchAlwaysRelocatable(const string &machine) const
    throw ()
{
    return machine == "ia64";
}

// -----------------------------------------------------------------------------
string KernelTool::archFromElfMachine(unsigned long long et_machine) const
    throw ()
{
    switch (et_machine) {
        case EM_386:    return "i386";
        case EM_PPC:    return "ppc";
        case EM_PPC64:  return "ppc64";
        case EM_S390:   return "s390";
        case EM_IA_64:  return "ia64";
        case EM_X86_64: return "x86_64";
        default:        return "unknown";
    }
}

// -----------------------------------------------------------------------------
Kconfig *KernelTool::retrieveKernelConfig() const
    throw (KError)
{
    Kconfig *kconfig = new Kconfig();

    // delete the object if we throw an exception
    try {
        // at first, search for the config on disk
        string dir, stripped;

        if (KernelTool::stripImageName(
                FileUtil::readlinkPath(m_kernel), dir, stripped)) {
            string config = FileUtil::pathconcat(dir, "config-" + stripped);
            Debug::debug()->dbg("Trying %s for config", config.c_str());
            if (FileUtil::exists(config)) {
                kconfig->readFromConfig(config);
                return kconfig;
            }
        }

        // and then extract the configuration
        kconfig->readFromKernel(*this);
        return kconfig;

    } catch (...) {
        delete kconfig;
        throw;
    }
}

// -----------------------------------------------------------------------------
string KernelTool::extractFromIKconfigBuffer(const char *buffer, size_t buflen)
    const
    throw (KError)
{
    Debug::debug()->trace("Kconfig::extractFromIKconfigBuffer(%p, %d)",
        buffer, buflen);

    ssize_t uncompressed_len = buflen * 20;
    auto_ptr<Bytef> uncompressed(new Bytef[uncompressed_len]);

    z_stream stream;
    stream.next_in = (Bytef *)buffer;
    stream.avail_in = buflen;

    stream.next_out = (Bytef *)uncompressed.get();
    stream.avail_out = uncompressed_len;

    stream.zalloc = NULL;
    stream.zfree = NULL;
    stream.opaque = NULL;

    int ret = inflateInit2(&stream, -MAX_WBITS);
    if (ret != Z_OK) {
        throw KError("inflateInit2() failed");
    }

    ret = inflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END) {
        throw KError("inflate() failed");
    }

    ret = inflateEnd(&stream);
    if (ret != Z_OK) {
        throw KError("Failed to uncompress the kernel configuration ("
            + Stringutil::number2string(ret) + ").");
    }

    uncompressed_len = stream.total_out;

    stringstream ss;
    uncompressed.get()[uncompressed_len] = '\0';
    return string((const char *)uncompressed.get());
}

// -----------------------------------------------------------------------------
string KernelTool::extractKernelConfigELF() const
    throw (KError)
{
    Debug::debug()->trace("Kconfig::extractKernelConfigELF()");

    off_t oret = lseek(m_fd, 0, SEEK_SET);
    if (oret == off_t(-1)) {
        throw KSystemError("lseek() failed", errno);
    }

    gzFile fp = gzdopen(dup(m_fd), "r");
    if (!fp) {
        throw KError(string("Opening '") + m_kernel + string("' failed."));
    }

    // we overlap the reads, that way searching for the pattern is easier
    char buffer[BUFSIZ];
    memset(buffer, 0, BUFSIZ);

    // search for the begin and end offests first
    off_t fileoffset = 0;
    ssize_t chars_read;
    off_t begin_offset = 0, end_offset = 0;
    while ((chars_read = gzread(fp, buffer+MAGIC_LEN, BUFSIZ-MAGIC_LEN)) > 0) {
        if (begin_offset == 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, MAGIC_START, MAGIC_LEN);
            if (pos > 0) {
                begin_offset = fileoffset + pos - MAGIC_LEN;
            }
        } else if (end_offset == 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, MAGIC_END, MAGIC_LEN);
            if (pos > 0) {
                end_offset = fileoffset + pos - MAGIC_LEN;
            }
        } else {
            break;
        }
        fileoffset += chars_read;
        memmove(buffer, buffer+BUFSIZ-MAGIC_LEN, MAGIC_LEN);
    }

    if (begin_offset < 0 || end_offset < 0) {
        gzclose(fp);
        throw KError("Cannot read configuration from " + m_kernel + ".");
    }

    // we need to read after the MAGIC_START and the gzip header
    begin_offset += MAGIC_LEN + 10;

    // read the configuration to a buffer
    if (gzseek(fp, begin_offset, SEEK_SET) == -1) {
        throw KError("gzseek() failed.");
    }
    ssize_t kernelconfig_len = end_offset - begin_offset;
    auto_ptr<char> kernelconfig(new char[kernelconfig_len]);

    chars_read = gzread(fp, kernelconfig.get(), kernelconfig_len);
    gzclose(fp);
    if (chars_read != kernelconfig_len) {
        throw KError("Cannot read IKCONFIG.");
    }

    return extractFromIKconfigBuffer(kernelconfig.get(), kernelconfig_len);
}

// -----------------------------------------------------------------------------
string KernelTool::extractKernelConfigbzImage() const
    throw (KError)
{
    Debug::debug()->trace("Kconfig::extractKernelConfigbzImage()");

    // that script helped me a lot
    // http://www.cs.caltech.edu/~weixl/research/fast-mon/scripts/extract-ikconfig

    char searchfor[4] = { 0x1f, 0x8b, 0x08, 0x0 };
    char buffer[BUFSIZ];
    ssize_t chars_read;

    off_t oret = lseek(m_fd, 0, SEEK_SET);
    if (oret == (off_t)-1) {
        throw KSystemError("lseek() failed", errno);
    }

    // because of overlapping
    memset(buffer, 0, BUFSIZ);

    off_t fileoffset = 0;
    off_t begin_offset = 0;
    while ((chars_read = read(m_fd, buffer+4, BUFSIZ-4)) > 0) {
        ssize_t pos = Util::findBytes(buffer, BUFSIZ, searchfor, 4);
        if (pos > 0) {
            begin_offset = fileoffset + pos - MAGIC_LEN;
            break;
        }
        memmove(buffer, buffer+BUFSIZ-MAGIC_LEN, MAGIC_LEN);
        fileoffset += chars_read;
    }

    if (begin_offset == 0) {
        throw KError("Magic 0x1f 0x8b 0x08 0x0 not found.");
    }

    begin_offset += 4;

    off_t ret = lseek(m_fd, begin_offset, SEEK_SET);
    if (ret == (off_t)-1) {
        throw KSystemError("lseek() failed", errno);
    }

    gzFile fp = gzdopen(dup(m_fd), "r");
    if (!fp) {
        throw KError("gzdopen() failed");
    }

    // we overlap the reads, that way searching for the pattern is easier
    memset(buffer, 0, BUFSIZ);

    // search for the begin and end offests first
    off_t end_offset = 0;
    begin_offset = 0;
    fileoffset = 0;
    while ((chars_read = gzread(fp, buffer+MAGIC_LEN, BUFSIZ-MAGIC_LEN)) > 0) {
        if (begin_offset == 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, MAGIC_START, MAGIC_LEN);
            if (pos > 0) {
                begin_offset = fileoffset + pos - MAGIC_LEN;
            }
        } else if (end_offset == 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, MAGIC_END, MAGIC_LEN);
            if (pos > 0) {
                end_offset = fileoffset + pos - MAGIC_LEN;
            }
        } else {
            break;
        }
        fileoffset += chars_read;
        memmove(buffer, buffer+BUFSIZ-MAGIC_LEN, MAGIC_LEN);
    }

    if (begin_offset < 0 || end_offset < 0) {
        gzclose(fp);
        throw KError("Cannot read configuration from " + m_kernel + ".");
    }

    // we need to read after the MAGIC_START and the gzip header
    begin_offset += MAGIC_LEN + 10;

    // read the configuration to a buffer
    if (gzseek(fp, begin_offset, SEEK_SET) == -1) {
        throw KError("gzseek() failed.");
    }
    ssize_t kernelconfig_len = end_offset - begin_offset;
    auto_ptr<char> kernelconfig(new char[kernelconfig_len]);

    chars_read = gzread(fp, kernelconfig.get(), kernelconfig_len);
    gzclose(fp);
    if (chars_read != kernelconfig_len) {
        throw KError("Cannot read IKCONFIG.");
    }

    return extractFromIKconfigBuffer(kernelconfig.get(), kernelconfig_len);
}

// -----------------------------------------------------------------------------
string KernelTool::extractKernelConfig() const
    throw (KError)
{
    Debug::debug()->trace("Kconfig::extractKernelConfig()");

    switch (getKernelType()) {
        case KernelTool::KT_ELF:
        case KernelTool::KT_ELF_GZ:
            return extractKernelConfigELF();

        case KernelTool::KT_X86:
            return extractKernelConfigbzImage();

        default:
            throw KError("Invalid kernel image: " + m_kernel);
    }
}

// -----------------------------------------------------------------------------
std::string KernelTool::toString() const
    throw ()
{
    return "[KernelTool] " + m_kernel;
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

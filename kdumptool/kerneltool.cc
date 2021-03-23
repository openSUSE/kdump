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
#include <endian.h>

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
#include "kernelpath.h"

using std::string;
using std::memset;
using std::unique_ptr;
using std::stringstream;
using std::list;

/* x86 boot header for bzImage */
#define X86_HEADER_OFF_START        0x202
#define X86_HEADER_OFF_VERSION      0x0206
#define X86_HEADER_OFF_RELOCATABLE  0x0234
#define X86_HEADER_OFF_MAGIC        0x53726448
#define X86_HEADER_RELOCATABLE_VER  0x0205

/* S/390 VM boot image */
#define S390_HEADER_OFF_IPLSTART    4
#define S390_HEADER_SIZE_IPLSTART   4
static const unsigned char S390_HEADER[] = {
  0x00, 0x08, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x18, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x00, 0x68, 0x60, 0x00, 0x00, 0x50,
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
  0x02, 0x00, 0x00, 0xf0, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x01, 0x40, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x01, 0x90, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x01, 0xe0, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x02, 0x30, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x02, 0x80, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x02, 0xd0, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x03, 0x20, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x03, 0x70, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x03, 0xc0, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x04, 0x10, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x04, 0x60, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x04, 0xb0, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x05, 0x00, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x05, 0x50, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x05, 0xa0, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x05, 0xf0, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x06, 0x40, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x06, 0x90, 0x60, 0x00, 0x00, 0x50,
  0x02, 0x00, 0x06, 0xe0, 0x20, 0x00, 0x00, 0x50,
};

#define MAGIC_START         "IKCFG_ST"
#define MAGIC_END           "IKCFG_ED"
#define MAGIC_LEN           8

const static unsigned char magic_start[] = MAGIC_START;
const static unsigned char magic_end[] = MAGIC_END;

// -----------------------------------------------------------------------------
KernelTool::KernelTool(const std::string &image)
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
KernelTool::KernelType KernelTool::getKernelType() const
{
    if (Util::isElfFile(m_fd)) {
        if (Util::isGzipFile(m_fd))
            return KT_ELF_GZ;
        else
            return KT_ELF;
    } else if (Util::isX86(Util::getArch())) {
        if (isX86Kernel())
            return KT_X86;
        else
            return KT_NONE;
    } else if (Util::getArch() == "s390x") {
        if (isS390Kernel())
            return KT_S390;
        else
            return KT_NONE;
    } else if (Util::getArch() == "aarch64") {
        if (isAarch64Kernel())
            return KT_AARCH64;
        else
            return KT_NONE;
    } else      
        return KT_NONE;
}

// -----------------------------------------------------------------------------
bool KernelTool::isRelocatable() const
{
    switch (getKernelType()) {
        case KernelTool::KT_ELF:
        case KernelTool::KT_ELF_GZ:
            return elfIsRelocatable();

        case KernelTool::KT_X86:
            return x86isRelocatable();
            break;

        case KernelTool::KT_S390:
            return true;

        case KernelTool::KT_AARCH64:
            return true;

        default:
            throw KError("Invalid kernel type.");
            break;
    }
}

// -----------------------------------------------------------------------------
bool KernelTool::isX86Kernel() const
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
bool KernelTool::isS390Kernel() const
{
    unsigned char buffer[sizeof(S390_HEADER)];

    /* check the magic number */
    if (lseek(m_fd, 0, SEEK_SET) == (off_t)-1) {
        throw KSystemError("IdentifyKernel::isS390Kernel: lseek to "
            "file start failed", errno);
    }

    int ret = read(m_fd, buffer, sizeof(buffer));
    if (ret < 0) {
        throw KSystemError("IdentifyKernel::isS390Kernel: read of magic "
            "start failed", errno);
    } else if (ret < (int)sizeof(buffer))
        return false;

    /* the iplstart address varies, so ignore it */
    memcpy(buffer + S390_HEADER_OFF_IPLSTART,
           S390_HEADER + S390_HEADER_OFF_IPLSTART,
           S390_HEADER_SIZE_IPLSTART);

    return memcmp(buffer, S390_HEADER, sizeof(S390_HEADER)) == 0;
}

// -----------------------------------------------------------------------------
bool KernelTool::isAarch64Kernel() const
{
    struct {
        uint32_t code0;         /* Executable code */
        uint32_t code1;         /* Executable code */
        uint64_t text_offset;   /* Image load offset, little endian */
        uint64_t image_size;    /* Effective Image size, little endian */
        uint64_t flags;         /* kernel flags, little endian */
        uint64_t res2;          /* reserved */
        uint64_t res3;          /* reserved */
        uint64_t res4;          /* reserved */
        uint32_t magic;         /* Magic number, little endian, "ARM\x64" */
        uint32_t res5;          /* reserved (used for PE COFF offset) */
    } buffer;

    /* check the magic number */
    if (lseek(m_fd, 0, SEEK_SET) == (off_t)-1) {
        throw KSystemError("IdentifyKernel::isAarch64Kernel: lseek to "
            "file start failed", errno);
    }

    int ret = read(m_fd, &buffer, sizeof(buffer));
    if (ret < 0) {
        throw KSystemError("IdentifyKernel::isAarch64Kernel: read of magic "
            "start failed", errno);
    } else if (ret < (int)sizeof(buffer))
        return false;

    return buffer.magic == 0x644d5241; /* little endian "ARM\x64" */
}

// -----------------------------------------------------------------------------
bool KernelTool::x86isRelocatable() const
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

    unsigned short machine;
    string arch;
    if (e_ident[EI_CLASS] == ELFCLASS32) {
        Elf32_Ehdr hdr;

        ret = gzread(fp, (unsigned char *)&hdr, sizeof(Elf32_Ehdr));
        if (ret != sizeof(Elf32_Ehdr)) {
            gzclose(fp);
            throw KSystemError("Couldn't read ELF header", errno);
        }

	if (e_ident[EI_DATA] == ELFDATA2LSB)
	    machine = le16toh(hdr.e_machine);
	else if (e_ident[EI_DATA] == ELFDATA2MSB)
	    machine = be16toh(hdr.e_machine);
	else
	    throw KError("elfIsRelocatable(): Invalid ELF data encoding");

    } else if (e_ident[EI_CLASS] == ELFCLASS64) {
        Elf64_Ehdr hdr;

        ret = gzread(fp, (unsigned char *)&hdr, sizeof(Elf64_Ehdr));
        if (ret != sizeof(Elf64_Ehdr)) {
            gzclose(fp);
            throw KSystemError("Couldn't read ELF header", errno);
        }

	if (e_ident[EI_DATA] == ELFDATA2LSB)
	    machine = le16toh(hdr.e_machine);
	else if (e_ident[EI_DATA] == ELFDATA2MSB)
	    machine = be16toh(hdr.e_machine);
	else
	    throw KError("elfIsRelocatable(): Invalid ELF data encoding");
    } else {
        throw KError("elfIsRelocatable(): Invalid ELF class");
    }
    arch = archFromElfMachine(machine);

    gzclose(fp);

    Debug::debug()->dbg("Detected arch %s", arch.c_str());

    return isArchAlwaysRelocatable(arch) ||
      (hasConfigRelocatable(arch) && isConfigRelocatable());
}

// -----------------------------------------------------------------------------
bool KernelTool::isArchAlwaysRelocatable(const string &machine) const
{
    return machine == "ia64" || machine == "aarch64";
}

// -----------------------------------------------------------------------------
bool KernelTool::hasConfigRelocatable(const string &machine) const
{
    return Util::isX86(machine) || machine == "ppc64" || machine == "ppc";
}

// -----------------------------------------------------------------------------
bool KernelTool::isConfigRelocatable() const
{
    try {
    Kconfig *kconfig = retrieveKernelConfig();
    KconfigValue kv = kconfig->get("CONFIG_RELOCATABLE");
    return (kv.getType() == KconfigValue::T_TRISTATE &&
	    kv.getTristateValue() == KconfigValue::ON);
    } catch (KError &e) {
	Debug::debug()->dbg("%s (assume non-relocatable)", e.what());
	return false;
    }
}

// -----------------------------------------------------------------------------
string KernelTool::archFromElfMachine(unsigned long long et_machine) const
{
    switch (et_machine) {
        case EM_386:    return "i386";
        case EM_PPC:    return "ppc";
        case EM_PPC64:  return "ppc64";
        case EM_S390:   return "s390";
        case EM_IA_64:  return "ia64";
        case EM_X86_64: return "x86_64";
        case EM_AARCH64: return "aarch64";
        default:        return "unknown";
    }
}

// -----------------------------------------------------------------------------
Kconfig *KernelTool::retrieveKernelConfig() const
{
    Kconfig *kconfig = new Kconfig();

    // delete the object if we throw an exception
    try {
        // at first, search for the config on disk
        KernelPath kpath(m_kernel);
        if (!kpath.version().empty()) {
            FilePath config = kpath.configPath();
            Debug::debug()->dbg("Trying %s for config", config.c_str());
            if (config.exists()) {
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
{
    Debug::debug()->trace("Kconfig::extractFromIKconfigBuffer(%p, %d)",
        buffer, buflen);

    ssize_t uncompressed_len = buflen * 20;
    unique_ptr<Bytef[]> uncompressed(new Bytef[uncompressed_len]);

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
            + StringUtil::number2string(ret) + ").");
    }

    uncompressed_len = stream.total_out;

    stringstream ss;
    uncompressed.get()[uncompressed_len] = '\0';
    return string((const char *)uncompressed.get());
}

// -----------------------------------------------------------------------------
string KernelTool::extractKernelConfigELF() const
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
    unsigned char buffer[BUFSIZ];
    memset(buffer, 0, BUFSIZ);

    // search for the begin and end offests first
    off_t fileoffset = 0;
    ssize_t chars_read;
    off_t begin_offset = -1, end_offset = -1;
    while ((chars_read = gzread(fp, buffer+MAGIC_LEN, BUFSIZ-MAGIC_LEN)) > 0) {
        if (begin_offset < 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, magic_start, MAGIC_LEN);
            if (pos > 0) {
                begin_offset = fileoffset + pos - MAGIC_LEN;
            }
        } else if (end_offset < 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, magic_end, MAGIC_LEN);
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
    unique_ptr<char[]> kernelconfig(new char[kernelconfig_len]);

    chars_read = gzread(fp, kernelconfig.get(), kernelconfig_len);
    gzclose(fp);
    if (chars_read != kernelconfig_len) {
        throw KError("Cannot read IKCONFIG.");
    }

    return extractFromIKconfigBuffer(kernelconfig.get(), kernelconfig_len);
}

// -----------------------------------------------------------------------------
string KernelTool::extractKernelConfigbzImage() const
{
    Debug::debug()->trace("Kconfig::extractKernelConfigbzImage()");

    // that script helped me a lot
    // http://www.cs.caltech.edu/~weixl/research/fast-mon/scripts/extract-ikconfig

    unsigned char searchfor[4] = { 0x1f, 0x8b, 0x08, 0x0 };
    unsigned char buffer[BUFSIZ];
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
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, magic_start, MAGIC_LEN);
            if (pos > 0) {
                begin_offset = fileoffset + pos - MAGIC_LEN;
            }
        } else if (end_offset == 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, magic_end, MAGIC_LEN);
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
    unique_ptr<char[]> kernelconfig(new char[kernelconfig_len]);

    chars_read = gzread(fp, kernelconfig.get(), kernelconfig_len);
    gzclose(fp);
    if (chars_read != kernelconfig_len) {
        throw KError("Cannot read IKCONFIG.");
    }

    return extractFromIKconfigBuffer(kernelconfig.get(), kernelconfig_len);
}

// -----------------------------------------------------------------------------
string KernelTool::extractKernelConfig() const
{
    Debug::debug()->trace("Kconfig::extractKernelConfig()");

    switch (getKernelType()) {
        case KernelTool::KT_ELF:
        case KernelTool::KT_ELF_GZ:
        case KernelTool::KT_S390:
        case KernelTool::KT_AARCH64:
            return extractKernelConfigELF();

        case KernelTool::KT_X86:
            return extractKernelConfigbzImage();

        default:
            throw KError("Invalid kernel image: " + m_kernel);
    }
}

// -----------------------------------------------------------------------------
std::string KernelTool::toString() const
{
    return "[KernelTool] " + m_kernel;
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

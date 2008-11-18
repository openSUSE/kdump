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

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include <libelf.h>
#include <gelf.h>

#include "kerneltool.h"
#include "util.h"
#include "global.h"
#include "debug.h"

using std::string;

/* x86 boot header for bzImage */
#define X86_HEADER_OFF_START        0x202
#define X86_HEADER_OFF_VERSION      0x0206
#define X86_HEADER_OFF_RELOCATABLE  0x0234
#define X86_HEADER_OFF_MAGIC        0x53726448
#define X86_HEADER_RELOCATABLE_VER  0x0205

// -----------------------------------------------------------------------------
KernelTool::KernelTool(const std::string &image)
    throw (KError)
    : m_fd(-1)
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
        throw KError("Invalid ELF class");
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

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

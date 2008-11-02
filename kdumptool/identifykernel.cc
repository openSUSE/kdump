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
#include <iostream>
#include <string>
#include <zlib.h>
#include <libelf.h>
#include <gelf.h>
#include <errno.h>
#include <fcntl.h>

#include "subcommand.h"
#include "debug.h"
#include "identifykernel.h"
#include "util.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

/* x86 boot header for bzImage */
#define X86_HEADER_OFF_START        0x202
#define X86_HEADER_OFF_VERSION      0x0206
#define X86_HEADER_OFF_RELOCATABLE  0x0234
#define X86_HEADER_OFF_MAGIC        0x53726448
#define X86_HEADER_RELOCATABLE_VER  0x0205


//{{{ IdentifyKernel -----------------------------------------------------------


// -----------------------------------------------------------------------------
IdentifyKernel::IdentifyKernel()
    throw ()
    : m_checkRelocatable(false), m_checkType(false)
{}

// -----------------------------------------------------------------------------
const char *IdentifyKernel::getName() const
    throw ()
{
    return "identify_kernel";
}

// -----------------------------------------------------------------------------
OptionList IdentifyKernel::getOptions() const
    throw ()
{
    OptionList list;

    list.push_back(Option("relocatable", 'r', OT_FLAG,
        "Checks if the kernel is relocatable"));
    list.push_back(Option("type", 't', OT_FLAG,
        "Prints the type of the kernel"));

    return list;
}

// -----------------------------------------------------------------------------
void IdentifyKernel::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace(__FUNCTION__);

    if (optionparser->getValue("relocatable").getFlag()) {
        Debug::debug()->dbg("Checking for kernel relocatability.");
        m_checkRelocatable = true;
    }
    if (optionparser->getValue("type").getFlag()) {
        Debug::debug()->dbg("Checking for kernel type.");
        m_checkType = true;
    }

    if (!m_checkType && !m_checkRelocatable)
        throw KError("You have to specify either the -r or the -t flag.");

    if (optionparser->getArgs().size() != 2)
        throw KError("You have to specify the kernel image for the "
            "identify_kernel subcommand.");

    m_kernelImage = optionparser->getArgs()[1];

    Debug::debug()->dbg("kernelimage = " + m_kernelImage);
}

// -----------------------------------------------------------------------------
void IdentifyKernel::execute()
    throw (KError)
{
    Debug::debug()->trace(__FUNCTION__);

    //
    // is it a kernel?
    //
    KernelType kt = getKernelType(m_kernelImage.c_str());
    if (kt == KT_NONE) {
        setErrorCode(NOT_A_KERNEL);
        throw KError("The specified file is not a kernel image.");
    }

    //
    // type checking
    //
    if (m_checkType) {
        switch (kt) {
            case KT_X86:
                cout << "x86" << endl;
                break;
            case KT_ELF:
                cout << "ELF" << endl;
                break;
            case KT_ELF_GZ:
                cout << "ELF gzip" << endl;
                break;
            default:
                throw KError("Unknown kernel type.");
        }
    }

    //
    // reloctable checking
    //
    if (m_checkRelocatable) {
        bool reloc;

        switch (kt) {
            case KT_ELF:
            case KT_ELF_GZ:
                reloc = checkElfFile(m_kernelImage.c_str());
                break;
            case KT_X86:
                reloc = checkArchFile(m_kernelImage.c_str());
            default:
                throw KError("Invalid kernel type.");
                break;
        }

        if (reloc)
            cout << "Relocatable" << endl;
        else {
            cout << "Not relocatable" << endl;
            setErrorCode(NOT_RELOCATABLE);
        }
    }
}

// -----------------------------------------------------------------------------
bool IdentifyKernel::isElfFile(const char *file)
    throw (KError)
{
    gzFile  fp = NULL;
    int     err;
    char    buffer[EI_MAG3+1];

    fp = gzopen(file, "r");
    if (!fp)
        throw KSystemError(string("Opening '") + file + string("' failed."),
            errno);

    err = gzread(fp, buffer, EI_MAG3+1);
    if (err != (EI_MAG3+1)) {
        gzclose(fp);
        throw KError("IdentifyKernel::isElfFile: Couldn't read bytes");
    }

    gzclose(fp);

    return buffer[EI_MAG0] == ELFMAG0 && buffer[EI_MAG1] == ELFMAG1 &&
            buffer[EI_MAG2] == ELFMAG2 && buffer[EI_MAG3] == ELFMAG3;
}

// -----------------------------------------------------------------------------
string IdentifyKernel::archFromElfMachine(unsigned long long et_machine)
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
bool IdentifyKernel::checkElfFile(const char *file)
    throw (KError)
{
    int             ret;
    gzFile          fp = NULL;
    unsigned char   e_ident[EI_NIDENT];
    int             reloc = false;

    fp = gzopen(file, "r");
    if (!fp)
        throw KSystemError("check_elf_file: Failed to open file", errno);

    ret = gzread(fp, e_ident, EI_NIDENT);
    if (ret != EI_NIDENT) {
        gzclose(fp);
        throw KSystemError("check_elf_file: Failed to read", errno);
    }

    if (gzseek(fp, 0, SEEK_SET) == (off_t)-1) {
        gzclose(fp);
        throw KSystemError("Seek failed", errno);
    }

    if (e_ident[EI_CLASS] == ELFCLASS32) {
        Elf32_Ehdr hdr;

        ret = gzread(fp, (unsigned char *)&hdr, sizeof(Elf32_Ehdr));
        if (ret != sizeof(Elf32_Ehdr)) {
            gzclose(fp);
            throw KSystemError("Couldn't read ELF header", errno);
        }

        m_arch = archFromElfMachine((unsigned long long)hdr.e_machine);

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

        m_arch = archFromElfMachine((unsigned long long)hdr.e_machine);
    }

    gzclose(fp);

    Debug::debug()->dbg("Detected arch %s", m_arch.c_str());

    return isArchAlwaysRelocatable(m_arch.c_str()) || reloc;
}

// -----------------------------------------------------------------------------
bool IdentifyKernel::checkArchFile(const char *filename)
    throw (KError)
{
    if (Util::isX86(Util::getArch()))
        return checkArchFileX86(filename);
    else
        return false;
}

// -----------------------------------------------------------------------------
bool IdentifyKernel::checkArchFileX86(const char *file)
    throw (KError)
{
    int             fd = -1;
    int             ret;
    unsigned char   buffer[BUFSIZ];
    off_t           off_ret;

    fd = open(file, O_RDONLY);
    if (fd < 0)
        throw KSystemError("IdentifyKernel::checkArchFileX86: open failed",
            errno);

    // check the magic number
    off_ret = lseek(fd, X86_HEADER_OFF_START, SEEK_SET);
    if (off_ret == (off_t)-1) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: lseek to "
            "X86_HEADER_OFF_START failed", errno);
    }

    ret = read(fd, buffer, 4);
    if (ret != 4) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: read of magic "
            "start failed", errno);
    }

    if (*((uint32_t *)buffer) != X86_HEADER_OFF_MAGIC) {
        close(fd);
        throw KError("This is not a kernel image");
    }

    // check the version
    off_ret = lseek(fd, X86_HEADER_OFF_VERSION, SEEK_SET);
    if (off_ret == (off_t)-1) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: lseek to "
            "X86_HEADER_OFF_VERSION failed", errno);
    }

    ret = read(fd, buffer, 2);
    if (ret != 2) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: read of version"
            " failed", errno);
    }

    // older versions are not relocatable
    if (*((uint16_t *)buffer) < X86_HEADER_RELOCATABLE_VER) {
        close(fd);
        return false;
    }

    // and check if the kernel is compiled to be relocatable
    off_ret = lseek(fd, X86_HEADER_OFF_RELOCATABLE, SEEK_SET);
    if (off_ret == (off_t)-1) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: lseek to "
            "X86_HEADER_OFF_RELOCATABLE failed", errno);
    }

    ret = read(fd, buffer, 1);
    if (ret != 1) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: read of "
            "relocatable bit failed", errno);
    }

    close(fd);

    return !!buffer[0];
}

// -----------------------------------------------------------------------------
bool IdentifyKernel::isArchAlwaysRelocatable(const char *machine)
    throw ()
{
    return string(machine) == "ia64";
}

// -----------------------------------------------------------------------------
bool IdentifyKernel::isX86Kernel(const char *file)
    throw (KError)
{
    int             fd = -1;
    int             ret;
    unsigned char   buffer[BUFSIZ];
    off_t           off_ret;

    fd = open(file, O_RDONLY);
    if (fd < 0)
        throw KSystemError("IdentifyKernel::isX86Kernel: open failed", errno);

    /* check the magic number */
    off_ret = lseek(fd, X86_HEADER_OFF_START, SEEK_SET);
    if (off_ret == (off_t)-1) {
        close(fd);
        throw KSystemError("IdentifyKernel::isX86Kernel: lseek to "
            "X86_HEADER_OFF_START failed", errno);
    }

    ret = read(fd, buffer, 4);
    if (ret != 4) {
        close(fd);
        throw KSystemError("IdentifyKernel::isX86Kernel: read of magic "
            "start failed", errno);
    }

    close(fd);

    return *((uint32_t *)buffer) == X86_HEADER_OFF_MAGIC;
}

// -----------------------------------------------------------------------------
IdentifyKernel::KernelType IdentifyKernel::getKernelType(const char *file)
    throw (KError)
{
    if (isElfFile(file)) {
        if (Util::isGzipFile(file))
            return KT_ELF_GZ;
        else
            return KT_ELF;
    } else {
        if (!Util::isX86(Util::getArch()))
            return KT_NONE;
        else {
            if (isX86Kernel(file))
                return KT_X86;
            else
                return KT_NONE;
        }
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

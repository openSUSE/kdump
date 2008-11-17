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
#include <cerrno>
#include <fcntl.h>

#include "subcommand.h"
#include "debug.h"
#include "identifykernel.h"
#include "util.h"
#include "kerneltool.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

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
    KernelTool::KernelType kt = KernelTool::getKernelType(m_kernelImage);
    if (kt == KernelTool::KT_NONE) {
        setErrorCode(NOT_A_KERNEL);
        throw KError("The specified file is not a kernel image.");
    }

    //
    // type checking
    //
    if (m_checkType) {
        switch (kt) {
            case KernelTool::KT_X86:
                cout << "x86" << endl;
                break;
            case KernelTool::KT_ELF:
                cout << "ELF" << endl;
                break;
            case KernelTool::KT_ELF_GZ:
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
            case KernelTool::KT_ELF:
            case KernelTool::KT_ELF_GZ:
                reloc = checkElfFile(m_kernelImage.c_str());
                break;
            case KernelTool::KT_X86:
                reloc = checkArchFile(m_kernelImage.c_str());
                break;
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
        return KernelTool::x86isRelocatable(filename);
    else
        return false;
}

// -----------------------------------------------------------------------------
bool IdentifyKernel::isArchAlwaysRelocatable(const char *machine)
    throw ()
{
    return string(machine) == "ia64";
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

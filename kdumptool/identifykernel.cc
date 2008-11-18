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
#include <cerrno>

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
    KernelTool kt(m_kernelImage);
    KernelTool::KernelType kerneltype = kt.getKernelType();
    if (kerneltype == KernelTool::KT_NONE) {
        setErrorCode(NOT_A_KERNEL);
        throw KError("The specified file is not a kernel image.");
    }

    //
    // type checking
    //
    if (m_checkType) {
        switch (kerneltype) {
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

        switch (kerneltype) {
            
        }

        if (reloc)
            cout << "Relocatable" << endl;
        else {
            cout << "Not relocatable" << endl;
            setErrorCode(NOT_RELOCATABLE);
        }
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

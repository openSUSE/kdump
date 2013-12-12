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
{
    m_options.push_back(new FlagOption("relocatable", 'r', &m_checkRelocatable,
        "Checks if the kernel is relocatable"));
    m_options.push_back(new FlagOption("type", 't', &m_checkType,
        "Prints the type of the kernel"));
}

// -----------------------------------------------------------------------------
const char *IdentifyKernel::getName() const
    throw ()
{
    return "identify_kernel";
}

// -----------------------------------------------------------------------------
void IdentifyKernel::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace(__FUNCTION__);

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

    KernelTool kt(m_kernelImage);

    if (m_checkType) {
        switch (kt.getKernelType()) {
            case KernelTool::KT_X86:
                cout << "x86" << endl;
                break;
            case KernelTool::KT_ELF:
                cout << "ELF" << endl;
                break;
            case KernelTool::KT_ELF_GZ:
                cout << "ELF gzip" << endl;
                break;
            case KernelTool::KT_S390:
                cout << "S390" << endl;
                break;
            default:
                throw KError("The specified file is not a kernel image.");
        }
    }

    if (m_checkRelocatable) {
        if (kt.isRelocatable())
            cout << "Relocatable" << endl;
        else {
            cout << "Not relocatable" << endl;
            setErrorCode(NOT_RELOCATABLE);
        }
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

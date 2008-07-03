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
#include "subcommand.h"
#include "debug.h"
#include "identifykernel.h"

//{{{ IdentifyKernel -----------------------------------------------------------

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

    list.push_back(Option("reloctable", 'r', OT_FLAG,
        "Checks if the kernel is relocatable"));
    list.push_back(Option("type", 't', OT_FLAG,
        "Prints the type of the kernel"));

    return list;
}

// -----------------------------------------------------------------------------
void IdentifyKernel::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
}

// -----------------------------------------------------------------------------
void IdentifyKernel::execute()
    throw (KError)
{
    Debug::debug()->trace(__FUNCTION__);
}

// vim: set sw=4 ts=4 fdm=marker et:

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

#include "subcommand.h"
#include "debug.h"
#include "read_ikconfig.h"
#include "kerneltool.h"
#include "util.h"

using std::cout;

//{{{ ReadIKConfig -------------------------------------------------------------

// -----------------------------------------------------------------------------
ReadIKConfig::ReadIKConfig()
{}

// -----------------------------------------------------------------------------
const char *ReadIKConfig::getName() const
{
    return "read_ikconfig";
}

// -----------------------------------------------------------------------------
bool ReadIKConfig::needsConfigfile() const
{
    return false;
}

// -----------------------------------------------------------------------------
void ReadIKConfig::parseArgs(const StringVector &args)
{
    Debug::debug()->trace(__FUNCTION__);

    if (args.size() != 1)
        throw KError("kernel image required.");

    m_file = args[0];
    Debug::debug()->dbg("file=%s", m_file.c_str());
}

// -----------------------------------------------------------------------------
void ReadIKConfig::execute()
{
    KernelTool kt(m_file);
    cout << kt.extractKernelConfig();
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

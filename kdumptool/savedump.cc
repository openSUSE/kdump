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
#include "savedump.h"
#include "util.h"

using std::string;

//{{{ IdentifyKernel -----------------------------------------------------------

// -----------------------------------------------------------------------------
SaveDump::SaveDump()
    throw ()
    : m_dump(DEFAULT_DUMP)
{}

// -----------------------------------------------------------------------------
const char *SaveDump::getName() const
    throw ()
{
    return "save_dump";
}

// -----------------------------------------------------------------------------
OptionList SaveDump::getOptions() const
    throw ()
{
    OptionList list;

    Debug::debug()->trace("SaveDump::getOptions()");

    list.push_back(Option("dump", 'u', OT_STRING,
        "Use the specified dump instead of " DEFAULT_DUMP "."));

    return list;

}

// -----------------------------------------------------------------------------
void SaveDump::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace("SaveDump::parseCommandline(%p)", optionparser);

    if (optionparser->getValue("dump").getType() != OT_INVALID)
        m_dump = optionparser->getValue("dump").getString();

    Debug::debug()->dbg("dump = " + m_dump);
}

// -----------------------------------------------------------------------------
void SaveDump::execute()
    throw (KError)
{
    Debug::debug()->trace(__FUNCTION__);

}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

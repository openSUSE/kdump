/*
 * (c) 2013, Petr Tesarik <ptesarik@suse.cz>, SUSE LINUX Products GmbH
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

#include "subcommand.h"
#include "debug.h"
#include "dumpconfig.h"
#include "configuration.h"

using std::cout;
using std::endl;

//{{{ DumpConfig ---------------------------------------------------------------

// -----------------------------------------------------------------------------
DumpConfig::DumpConfig()
    throw ()
{}

// -----------------------------------------------------------------------------
const char *DumpConfig::getName() const
    throw ()
{
    return "dump_config";
}

// -----------------------------------------------------------------------------
void DumpConfig::execute()
    throw (KError)
{
    Debug::debug()->trace("DumpConfig::execute()");

    Configuration *config = Configuration::config();

    ConfigOptionIterator it,
	begin = config->optionsBegin(),
	end = config->optionsEnd();

    for (it = begin; it != end; ++it) {
	cout << (*it)->name() << "='" << (*it)->valueAsString() << "'" << endl;
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

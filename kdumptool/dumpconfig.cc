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

using std::string;
using std::cout;
using std::endl;

// -----------------------------------------------------------------------------
static bool is_shell_safe(const char c)
{
    return (c >= '0' && c <= '9') ||
	(c >= 'A' && c <= 'Z') ||
	(c >= 'a' && c <= 'z') ||
	(c == '_' || c == '/' || c == '@' ||
	 c == '.' || c == ',' || c == ':');
}

// -----------------------------------------------------------------------------
static string quote_shell(const string &str)
{
    string ret;
    bool quotes_needed = false;
    for (string::const_iterator it = str.begin(); it != str.end(); ++it) {
	ret.push_back(*it);
	if (!is_shell_safe(*it))
	    quotes_needed = true;
	if (*it == '\'')
	    ret.append("\\\'\'");
    }
    if (quotes_needed) {
	ret.insert(0, "\'");
	string::iterator last = ret.end();
	--last;
	if (*last == '\'')
	    ret.erase(last);
	else
	    ret.push_back('\'');
    }
    return ret;
}

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
	cout << (*it)->name() << "="
	     << quote_shell((*it)->valueAsString()) << endl;
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

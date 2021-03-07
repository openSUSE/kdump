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

#include "fileutil.h"
#include "subcommand.h"
#include "debug.h"
#include "print_target.h"
#include "util.h"
#include "configuration.h"
#include "rootdirurl.h"
#include "stringutil.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

//{{{ PrintTarget --------------------------------------------------------------------

// -----------------------------------------------------------------------------
PrintTarget::PrintTarget()
    : m_rootdir()
{
    m_options.push_back(new StringOption("root", 'R', &m_rootdir,
        "Use the specified root directory instead of /"));
}

// -----------------------------------------------------------------------------
const char *PrintTarget::getName() const
{
    return "print_target";
}

// -----------------------------------------------------------------------------
void PrintTarget::execute()
{
    Debug::debug()->trace("PrintTarget::execute()");
    Debug::debug()->dbg("root: %s", m_rootdir.c_str());

    Configuration *config = Configuration::config();

    std::istringstream iss(config->KDUMP_SAVEDIR.value());
    string elem;
    bool first = true;
    while (iss >> elem) {
        RootDirURL url(elem, m_rootdir);
        if (first)
            first = false;
        else
	    cout << endl;
	print_one(url);
    }
}

// -----------------------------------------------------------------------------
void PrintTarget::print_one(RootDirURL &parser)
{
    string port;

    if (parser.getPort() != -1)
        port = StringUtil::number2string(parser.getPort());

    cout << "Protocol:   " << parser.getProtocolAsString() << endl;
    cout << "URL:        " << parser.getURL() << endl;
    cout << "Username:   " << parser.getUsername() << endl;
    cout << "Password:   " << parser.getPassword() << endl;
    cout << "Host:       " << parser.getHostname() << endl;
    cout << "Port:       " << port << endl;
    cout << "Path:       " << parser.getPath() << endl;
    cout << "Realpath:   " << parser.getRealPath() << endl;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

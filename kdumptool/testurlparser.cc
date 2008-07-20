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
#include <string>
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <fstream>

#include "global.h"
#include "kdumptool.h"
#include "util.h"
#include "stringutil.h"
#include "process.h"
#include "debug.h"
#include "urlparser.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::ifstream;
using std::ofstream;

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    string url;

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " url" << endl;
        return EXIT_FAILURE;
    }

    url = argv[1];

    Debug::debug()->setStderrLevel(Debug::DL_TRACE);
    try {
        URLParser urlp;
        urlp.parseURL(url);
        cout << "Prot\t"    << urlp.getProtocolAsString() << endl;
        cout << "Host\t"    << urlp.getHostname() << endl;
        cout << "Port\t"    << urlp.getPort() << endl;
        cout << "User\t"    << urlp.getUsername() << endl;
        cout << "Pass\t"    << urlp.getPassword() << endl;
        cout << "Path\t"    << urlp.getPath() << endl;

    } catch (const std::exception &ex) {
        cerr << "Fatal exception: " << ex.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

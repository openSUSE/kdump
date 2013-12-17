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
#include <iomanip>

#include "global.h"
#include "kdumptool.h"
#include "util.h"
#include "stringutil.h"
#include "fileutil.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::boolalpha;

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    FilePath path;

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " path" << endl;
        return EXIT_FAILURE;
    }

    path = argv[1];

    try {
        bool isLink = path.isSymlink();
        cout << "Symbolic Link?\t"     << boolalpha << isLink << endl;

        if (isLink) {
            string readlink = path.readLink();
            cout << "Readlink\t"  << readlink << endl;
        }

        string canonical = path.getCanonicalPath();
        cout << "Canonical\t" << canonical << endl;

    } catch (const std::exception &ex) {
        cerr << "Fatal exception: " << ex.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

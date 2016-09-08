/*
 * (c) 2014, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
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

#include <cstdlib>
#include <string>
#include <algorithm>
#include <iostream>
#include <iterator>

#include "global.h"
#include "kdumptool.h"
#include "fileutil.h"
#include "debug.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::ostream_iterator;
using std::copy;

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " testdir mode" << endl;
        return EXIT_FAILURE;
    }

    FilePath dir(argv[1]);

    string mode(argv[2]);
    ListDirFilter *filt;
    if (mode == "all")
	filt = new FilterDots();
    else if (mode == "dirs")
	filt = new FilterDotsAndNondirs();
    else if (mode == "kdumpdirs")
	filt = new FilterKdumpDirs();
    else {
	cerr << "Mode must be one of: 'all', 'dirs', 'kdumpdirs'" << endl;
	return EXIT_FAILURE;
    }

    Debug::debug()->setStderrLevel(Debug::DL_TRACE);

    try {
	StringVector v = dir.listDir(*filt);
	copy(v.begin(), v.end(), ostream_iterator<string>(cout, "\n"));
    } catch (const std::exception &ex) {
        cerr << "Fatal exception: " << ex.what() << endl;
	return EXIT_FAILURE;
    }

    delete filt;
    return EXIT_SUCCESS;
}

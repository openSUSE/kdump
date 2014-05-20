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
#include <iostream>
#include <cstdlib>

#include "global.h"
#include "stringutil.h"
#include "debug.h"

using std::cout;
using std::cerr;
using std::endl;

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    Debug::debug()->setStderrLevel(Debug::DL_TRACE);
    bool formenc = false;

    try {
	int i = 1;
	if (i < argc && KString(argv[i]) == "-f") {
	    formenc = true;
	    ++i;
	}

	bool addspace = false;
	while (i < argc) {
	    KString arg(argv[i]);
	    if (addspace)
		cout << ' ';
	    cout << arg.decodeURL(formenc);
	    addspace = true;
	    ++i;
	}
	cout << endl;

    } catch(const std::exception &ex) {
	cerr << "Fatal exception: " << ex.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

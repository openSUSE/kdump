/*
 * (c) 2013, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
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
#include <iostream>
#include <string>

#include "global.h"
#include "debug.h"
#include "stringutil.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int result = EXIT_SUCCESS;

    Debug::debug()->setStderrLevel(Debug::DL_TRACE);
    try {
        KString empty;
        KString s("prefixed and suffixed");
        KString s_copy(s);
        KString s_append(s);
        s_append.append(" and something");
        KString s_prepend(s);
        s_prepend.insert(0, "something and ");

        if (!empty.startsWith("")) {
            cerr << "Empty string does not start with an empty string" << endl;
            result = EXIT_FAILURE;
        }

        if (empty.startsWith("bogus")) {
            cerr << "Empty string starts with a bogus string" << endl;
            result = EXIT_FAILURE;
        }

        if (!s.startsWith("prefix")) {
            cerr << "Prefix not recognized" << endl;
            result = EXIT_FAILURE;
        }

        if (s.startsWith("bogus")) {
            cerr << "Prefix matched incorrectly" << endl;
            result = EXIT_FAILURE;
        }

        if (!s.startsWith(empty)) {
            cerr << "Empty string is not a prefix" << endl;
            result = EXIT_FAILURE;
        }

        if (!s.startsWith(s_copy)) {
            cerr << "Full match failed as prefix" << endl;
            result = EXIT_FAILURE;
        }

        if (s.startsWith(s_append)) {
            cerr << "Shorter string starts with longer string" << endl;
            result = EXIT_FAILURE;
        }

        if (!empty.endsWith("")) {
            cerr << "Empty string does not end with an empty string" << endl;
            result = EXIT_FAILURE;
        }

        if (empty.endsWith("bogus")) {
            cerr << "Empty string starts with a bogus string" << endl;
            result = EXIT_FAILURE;
        }

        if (!s.endsWith("suffixed")) {
            cerr << "Suffix not recognized" << endl;
            result = EXIT_FAILURE;
        }

        if (s.endsWith("bogus")) {
            cerr << "Suffix matched incorrectly" << endl;
            result = EXIT_FAILURE;
        }

        if (!s.endsWith(empty)) {
            cerr << "Empty string is not a suffix" << endl;
            result = EXIT_FAILURE;
        }

        if (!s.endsWith(s_copy)) {
            cerr << "Full match failed as suffix" << endl;
            result = EXIT_FAILURE;
        }

        if (s.endsWith(s_prepend)) {
            cerr << "Shorter string ends with longer string" << endl;
            result = EXIT_FAILURE;
        }

    } catch (const std::exception &ex) {
        cerr << "Fatal exception: " << ex.what() << endl;
        result = EXIT_FAILURE;
    }

    return result;
}

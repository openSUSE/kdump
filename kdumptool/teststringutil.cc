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

//{{{ TestRun -----------------------------------------------------------------

class TestRun {
    int m_result;

public:
    TestRun()
        : m_result(EXIT_SUCCESS)
    { }

    int result(void)
    { return m_result; }

    template<typename check_fn>
    void check(const char *what, check_fn fn);
};

// -----------------------------------------------------------------------------
template<typename check_fn>
void TestRun::check(const char *what, check_fn fn)
{
    cout << what << ": ";
    try {
        if (fn()) {
            cout << "OK";
        } else {
            cout << "FAILED";
            m_result = EXIT_FAILURE;
        }
        cout << endl;
    } catch (KError &e) {
        cout << "EXCEPTION" << endl;
        cerr << e.what() << endl;
        m_result = EXIT_FAILURE;
    }
}
//}}}

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int result = EXIT_SUCCESS;

    Debug::debug()->setStderrLevel(Debug::DL_TRACE);
    try {
        TestRun test;

        // prepare string constants
        KString const empty;
        KString const s("prefixed and suffixed");
        KString const s_copy(s);
        KString s_append(s);
        s_append.append(" and something");
        KString s_prepend(s);
        s_prepend.insert(0, "something and ");

        // KString::endsWith()
        test.check("Empty string starts with an empty string",
                   [&empty]() {
                       return empty.startsWith("");
                   });

        test.check("Empty string does not start with a bogus string",
                   [&empty]() {
                       return !empty.startsWith("bogus");
                   });

        test.check("Prefix string matches",
                   [&s]() {
                       return s.startsWith("prefix");
                   });

        test.check("Bogus prefix does not match",
                   [&s]() {
                       return !s.startsWith("bogus");
                   });

        test.check("Empty string is a prefix",
                   [&s, &empty]() {
                       return s.startsWith(empty);
                   });

        test.check("Identical string is a prefix",
                   [&s, &s_copy]() {
                       return s.startsWith(s_copy);
                   });

        test.check("String does not start with a longer string",
                   [&s, &s_append]() {
                       return !s.startsWith(s_append);
                   });

        // KString::endsWith()
        test.check("Empty string ends with an empty string",
                   [&empty]() {
                       return empty.endsWith("");
                   });

        test.check("Empty string does not end with a bogus string",
                   [&empty]() {
                       return !empty.endsWith("bogus");
                   });

        test.check("Suffix string matches",
                   [&s]() {
                       return s.endsWith("suffixed");
                   });

        test.check("Bogus suffix does not match",
                   [&s]() {
                       return !s.endsWith("bogus");
                   });

        test.check("Empty string is a suffix",
                   [&s, &empty]() {
                       return s.endsWith(empty);
                   });

        test.check("Identical string is a prefix",
                   [&s, &s_copy]() {
                       return s.endsWith(s_copy);
                   });

        test.check("String does not end with a longer string",
                   [&s, &s_prepend]() {
                       return !s.endsWith(s_prepend);
                   });

        result = test.result();

    } catch (const std::exception &ex) {
        cerr << "Fatal exception: " << ex.what() << endl;
        result = EXIT_FAILURE;
    }

    return result;
}

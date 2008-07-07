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

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::ifstream;
using std::ofstream;

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    string testdata, output1, output2;

    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " testdata output1 output2" << endl;
        return EXIT_FAILURE;
    }

    testdata = argv[1];
    output1 = argv[2];
    output2 = argv[3];

    Debug::debug()->setLevel(Debug::DL_TRACE);
    try {
        // args for cat
        StringVector v;
        v.push_back("-");

        Debug::debug()->info("1st test: %s -> %s",
            testdata.c_str(), output1.c_str());
        Process p;

        ByteVector stdoutV;
        p.setStdinFile(testdata.c_str());
        p.setStdoutBuffer(&stdoutV);
        p.execute("cat", v);

        ofstream fout(output1.c_str());
        fout << Stringutil::bytes2str(stdoutV);
        fout.close();

        Debug::debug()->info("1st test: %s -> %s",
            testdata.c_str(), output2.c_str());

        Process p2;
        ifstream fin(testdata.c_str());
        string s;
        if (!fin)
            cerr << "Eingabe-Datei kann nicht geöffnet werden" << endl;
        else {
            char c;
            while (fin.get(c))
                s += c;
        }
        ByteVector bv = Stringutil::str2bytes(s);
        p2.setStdinBuffer(&bv);
        p2.setStdoutFile(output2.c_str());
        p2.execute("cat", v);

    } catch (const std::exception &ex) {
        cerr << "Fatal exception: " << ex.what() << endl;
    }

    return EXIT_FAILURE;
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

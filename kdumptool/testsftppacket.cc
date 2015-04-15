/*
 * (c) 2015, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
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
#include "sshtransfer.h"
#include "debug.h"

using std::cout;
using std::cerr;
using std::setw;
using std::endl;

// -----------------------------------------------------------------------------
static void
dumpvec(ByteVector const &bv)
{
    cout << std::hex << std::setfill('0');
    ByteVector::const_iterator it = bv.begin();
    if (it != bv.end()) {
	cout << setw(2) << unsigned(*it);
	++it;
    }
    while (it != bv.end()) {
	cout << ' ' << setw(2) << unsigned(*it);
	++it;
    }
    cout << endl;
}

// -----------------------------------------------------------------------------
static unsigned long long
parseval(const char *str, unsigned maxdigits)
{
    unsigned long long ret = 0;
    unsigned i;

    for (i = 0; i < maxdigits; ++i) {
	if (!str[i])
	    break;
	ret <<= 4;
	ret |= Stringutil::hex2int(str[i]);
    }
    if (str[i])
	throw KError(KString("Number too big: '") + str + "'");

    return ret;
}

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    Debug::debug()->setStderrLevel(Debug::DL_TRACE);

    try {
	SFTPPacket pkt;

	int i;
	for (i = 1; i < argc; ++i) {
	    char *arg = argv[i];

	    switch (arg[0]) {
	    case 'd':
		dumpvec(pkt.data());
		break;

	    case 'u':
		dumpvec(pkt.update());
		break;

	    case 'b':
		if (arg[1])
		    pkt.addByte(parseval(arg + 1, 2));
		break;

	    case 'w':
		if (arg[1])
		    pkt.addInt32(parseval(arg + 1, 8));
		break;

	    case 'l':
		if (arg[1])
		    pkt.addInt64(parseval(arg + 1, 16));
		break;

	    case '\0':
		// Ignore empty arguments
		break;

	    default:
		throw KError("Invalid format specifier");
	    }
	}

    } catch(const std::exception &ex) {
	cerr << "Fatal exception: " << ex.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

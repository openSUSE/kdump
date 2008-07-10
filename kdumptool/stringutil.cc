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
#include <sstream>
#include <errno.h>
#include <cstring>

#include "stringutil.h"
#include "global.h"

using std::string;
using std::stringstream;
using std::strcpy;

//{{{ Stringutil ---------------------------------------------------------------

// -----------------------------------------------------------------------------
int Stringutil::string2number(const std::string &string)
    throw ()
{
    int ret;
    stringstream ss;
    ss << string;
    ss >> ret;
    return ret;
}

// -----------------------------------------------------------------------------
char ** Stringutil::stringv2charv(const StringVector &strv)
    throw (KError)
{
    char **ret;

    // allocate the outer array
    ret = new char *[strv.size() + 1];
    if (!ret)
        throw KSystemError("Memory allocation failed", errno);
    ret[strv.size()] = NULL;

    // fill the inner stuff
    int i = 0;
    for (StringVector::const_iterator it = strv.begin();
            it != strv.end();
            ++it) {
        // don't use strdup() to avoid mixing of new and malloc
        ret[i] = new char[(*it).size() + 1];
        strcpy(ret[i], (*it).c_str());
        i++;
    }

    return ret;
}

/* -------------------------------------------------------------------------- */
ByteVector Stringutil::str2bytes(const string &string)
    throw ()
{
    ByteVector ret;
    const char *cstr = string.c_str();

    ret.insert(ret.begin(), cstr, cstr + string.size());

    return ret;
}

/* -------------------------------------------------------------------------- */
string Stringutil::bytes2str(const ByteVector &bytes)
    throw ()
{
    return string(bytes.begin(), bytes.end());
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

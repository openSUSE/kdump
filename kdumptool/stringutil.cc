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
#include <sstream>
#include <iomanip>

#include "stringutil.h"
#include "global.h"
#include "debug.h"

using std::string;
using std::stringstream;
using std::strcpy;
using std::hex;
using std::setfill;
using std::setw;

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

// -----------------------------------------------------------------------------
string Stringutil::bytes2hexstr(const char *bytes, size_t len, bool colons)
    throw ()
{
    stringstream ss;

    for (size_t i = 0; i < len; i++) {
        ss << setw(2) << setfill('0') << hex << int(int(bytes[i]) & 0xff);
        if (colons && i != (len-1))
            ss << ":";
    }

    return ss.str();
}

// -----------------------------------------------------------------------------
string Stringutil::trim(const std::string &string, const char *chars)
    throw ()
{
    string::size_type start = string.find_first_not_of(chars);
    if (start == string::npos)
        return "";

    string::size_type end = string.find_last_not_of(chars);

    return string.substr(start, end-start+1);
}

// -----------------------------------------------------------------------------
string Stringutil::ltrim(const std::string &string, const char *chars)
    throw ()
{
    string::size_type start = string.find_first_not_of(chars);
    if (start == string::npos)
        return "";

    return string.substr(start);
}

// -----------------------------------------------------------------------------
string Stringutil::rtrim(const std::string &string, const char *chars)
    throw ()
{
    string::size_type end = string.find_last_not_of(chars);
    if (end == string::npos)
        return "";

    return string.substr(0, end+1);
}

// -----------------------------------------------------------------------------
string Stringutil::vector2string(const StringVector &vector,
                                 const char *delimiter)
    throw ()
{
    string ret;

    for (size_t i = 0; i < vector.size(); i++) {
        ret += vector[i];
        if (i != vector.size() - 1)
            ret += delimiter;
    }

    return ret;
}

// -----------------------------------------------------------------------------
StringVector Stringutil::splitlines(const string &str)
    throw ()
{
    StringVector ret;
    stringstream ss;
    ss << str;

    string s;
    while (getline(ss, s))
        ret.push_back(s);

    return ret;
}

// -----------------------------------------------------------------------------
bool Stringutil::startsWith(const string &long_string,
                            const string &part)
    throw ()
{
    return strncmp(long_string.c_str(), part.c_str(), part.size()) == 0;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

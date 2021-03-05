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
#include <cerrno>
#include <sstream>
#include <iomanip>
#include <ctime>

#include "stringutil.h"
#include "global.h"
#include "debug.h"

using std::string;
using std::stringstream;

//{{{ Stringutil ---------------------------------------------------------------

// -----------------------------------------------------------------------------
int Stringutil::string2number(const std::string &string)
{
    int ret;
    stringstream ss;
    ss << string;
    ss >> ret;
    return ret;
}

// -----------------------------------------------------------------------------
long long Stringutil::string2llong(const std::string &string)
{
    long long ret;
    stringstream ss;
    ss << string;
    ss >> ret;
    return ret;
}

/* -------------------------------------------------------------------------- */
ByteVector Stringutil::str2bytes(const string &string)
{
    ByteVector ret;
    const char *cstr = string.c_str();

    ret.insert(ret.begin(), cstr, cstr + string.size());

    return ret;
}

// -----------------------------------------------------------------------------
string Stringutil::formatUnixTime(const char *formatstring, time_t value)
{
    char buffer[BUFSIZ];

    struct tm mytime;
    strftime(buffer, BUFSIZ, formatstring, localtime_r(&value, &mytime));

    return string(buffer);
}

// -----------------------------------------------------------------------------
string Stringutil::formatCurrentTime(const char *formatstring)
{
    return formatUnixTime(formatstring, time(NULL));
}

// -----------------------------------------------------------------------------
int Stringutil::hex2int(char c)
{
    if (c >= '0' && c <= '9')
	return c - '0';
    if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
    throw KError(string("Stringutil::hex2int: '") + c + "' is not a hex digit");
}

//}}}
//{{{ KString ------------------------------------------------------------------

// -----------------------------------------------------------------------------
bool KString::isNumber()
{
    iterator it = begin();

    // leading sign
    if (it != end() && (*it == '-' || *it == '+'))
        ++it;

    if (it == end())
        return false;
    do {
        if (!isdigit(*it))
            return false;
    } while (++it != end());
    return true;
}

// -----------------------------------------------------------------------------
bool KString::isHexNumber()
{
    iterator it = begin();

    if (it == end())
        return false;
    do {
        if (!isxdigit(*it))
          return false;
    } while (++it != end());
    return true;
}

// -----------------------------------------------------------------------------
KString& KString::trim(const char *chars)
{
    return rtrim(chars).ltrim(chars);
}

// -----------------------------------------------------------------------------
KString& KString::ltrim(const char *chars)
{
    erase(0, find_first_not_of(chars));
    return *this;
}

// -----------------------------------------------------------------------------
KString& KString::rtrim(const char *chars)
{
    erase(find_last_not_of(chars) + 1);
    return *this;
}

// -----------------------------------------------------------------------------
bool KString::startsWith(const string &part) const
{
    return compare(0, part.length(), part) == 0;
}

// -----------------------------------------------------------------------------
bool KString::endsWith(const string &part) const
{
    return length() >= part.length()
        ? compare(length() - part.length(), part.length(), part) == 0
        : false;
}

// -----------------------------------------------------------------------------
StringVector KString::split(char split)
{
    StringVector ret;

    size_type start = 0;
    size_type next = find(split);
    while (next != npos) {
        ret.emplace_back(*this, start, next - start);
        start = next + 1;
        next = find(split, start);
    }

    // rest
    ret.emplace_back(*this, start);

    return ret;
}

// -----------------------------------------------------------------------------
KString &KString::decodeURL(bool formenc)
{
    iterator src, dst;
    for (src = dst = begin(); src != end(); ++src) {
	char c1, c2;
	if (*src == '%' && end() - src >= 2 &&
	    isxdigit(c1 = src[1]) && isxdigit(c2 = src[2])) {
	    *dst++ = (Stringutil::hex2int(c1) << 4) |
		Stringutil::hex2int(c2);
	    src += 2;
	} else if (formenc && *src == '+')
	    *dst++ = ' ';
	else
	    *dst++ = *src;
    }
    resize(dst - begin());
    return *this;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

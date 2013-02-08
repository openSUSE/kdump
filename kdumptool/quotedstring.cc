/*
 * (c) 2013, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your opti) any later version.
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

#include "quotedstring.h"

using std::string;

//{{{ QuotedString -------------------------------------------------------------

// -----------------------------------------------------------------------------
QuotedString::~QuotedString()
{
    // Nothing to do, but a virtual destructor for an abstract base class
    // is needed, otherwise dynamically deleting a pointer to the base class
    // is undefined.
}

//{{{ ShellQuotedString --------------------------------------------------------

// -----------------------------------------------------------------------------
static bool is_shell_safe(const char c)
{
    return (c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c == '_' || c == '/' || c == '@' ||
         c == '.' || c == ',' || c == ':');
}

// -----------------------------------------------------------------------------
string ShellQuotedString::quoted(void) const
{
    string ret;
    bool quotes_needed = false;
    for (const_iterator it = begin(); it != end(); ++it) {
        ret.push_back(*it);
        if (!is_shell_safe(*it))
            quotes_needed = true;
        if (*it == '\'')
            ret.append("\\\'\'");
    }
    if (quotes_needed) {
        ret.insert(0, "\'");
        iterator last = ret.end();
        --last;
        if (*last == '\'')
            ret.erase(last);
        else
            ret.push_back('\'');
    }
    return ret;
}

//}}}

//{{{ KernelQuotedString -------------------------------------------------------

// -----------------------------------------------------------------------------
static bool is_kernel_space(const char c)
{
    // follow the definition from lib/ctype.c in the Linux kernel tree
    return (c >= 9 && c <= 13) || c == 32 || c == (char)160;
}

// -----------------------------------------------------------------------------
string KernelQuotedString::quoted(void) const
{
    string ret;
    bool quotes_needed = false;
    for (const_iterator it = begin(); it != end(); ++it) {
        if (is_kernel_space(*it))
            quotes_needed = true;
        if (*it == '\"')
            ret.append("\\042");
        else if (*it == '\\')
            ret.append("\\\\");
        else
            ret.push_back(*it);
    }
    if (quotes_needed) {
        ret.insert(0, "\"");
        ret.push_back('\"');
    }
    return ret;
}

//}}}

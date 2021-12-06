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
#ifndef QUOTEDSTRING_H
#define QUOTEDSTRING_H

#include <string>

//{{{ QuotedString -------------------------------------------------------------

/**
 * QuotedString is an abstract base class which extends std::string with
 * a method to get a quoted version of the string without saying how the
 * quoting should be implemented.
 */
class QuotedString : public std::string {

    public:
        explicit QuotedString()
        : std::string()
        { }

        QuotedString(const std::string& str)
        : std::string(str)
        { }

        QuotedString(const std::string& str, size_t pos, size_t len = npos)
        : std::string(str, pos, len)
        { }

        QuotedString(const char* s)
        : std::string(s)
        { }

        QuotedString(const char* s, size_t n)
        : std::string(s, n)
        { }

        QuotedString(size_t n, char c)
        : std::string(n, c)
        { }

        template <class InputIterator>
        QuotedString(InputIterator first, InputIterator last)
        : std::string(first, last)
        { }

        virtual ~QuotedString();

        virtual std::string quoted(void) const = 0;
};

//}}}

//{{{ ShellQuotedString --------------------------------------------------------

class ShellQuotedString : public QuotedString {

    public:
        explicit ShellQuotedString()
        : QuotedString()
        { }

        ShellQuotedString(const std::string& str)
        : QuotedString(str)
        { }

        ShellQuotedString(const std::string& str,
                          size_t pos, size_t len = npos)
        : QuotedString(str, pos, len)
        { }

        ShellQuotedString(const char* s)
        : QuotedString(s)
        { }

        ShellQuotedString(const char* s, size_t n)
        : QuotedString(s, n)
        { }

        ShellQuotedString(size_t n, char c)
        : QuotedString(n, c)
        { }

        template <class InputIterator>
        ShellQuotedString(InputIterator first, InputIterator last)
        : QuotedString(first, last)
        { }

        virtual std::string quoted(void) const;
};

//}}}

#endif /* QUOTEDSTRING_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

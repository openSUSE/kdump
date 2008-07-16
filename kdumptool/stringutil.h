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
#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include <string>
#include <sstream>

#include "global.h"

//{{{ Stringutil ---------------------------------------------------------------

/**
 * String helper functions.
 */
class Stringutil {

    public:
        /**
         * Transforms a numeric value into a string.
         *
         * @param[in] number the number to transform
         * @return the string
         */
        template <typename numeric_type>
        static std::string number2string(numeric_type number)
        throw ();

        /**
         * Parses a number.
         *
         * @param[in] string string that should be parsed
         * @return the number
         */
        static int string2number(const std::string &string)
        throw ();

        /**
         * Converts an C++ string vector to an C string array (dynamically
         * allocated)
         *
         * @param[in] the string vector
         * @return an dynamically allocated string, which should be freed
         *         with Util::freev()
         *
         * @throw KError if memory allocation failed
         */
        static char ** stringv2charv(const StringVector &strv)
        throw (KError);

        /**
         * Converts a string to a byte vector.
         *
         * @param[in] string the string
         * @return the byte vector
         */
        static ByteVector str2bytes(const std::string &string)
        throw ();

        /**
         * Converts a byte vector to a string.
         *
         * @param[in] bytes the byte vector
         * @return the string
         */
        static std::string bytes2str(const ByteVector &bytes)
        throw ();

        /**
         * Converts a raw byte buffer to hex numbers.
         *
         * @param[in] bytes the byte buffer
         * @param[in] len the lenght of the byte buffer
         * @param[in] spaces @c true if the code should insert a space between
         *            a hex pair, @c false otherwise
         *
         * @return the hex string
         */
        static std::string bytes2hexstr(const char *bytes, size_t len,
            bool colons = false)
        throw ();

        /**
         * Remove trailing or leading stuff.
         *
         * @param[in] string the string
         * @param[in] chars the characters to remove (default: white space)
         * @return the trimmed string
         */
        static std::string trim(const std::string &string,
                                const char *chars = " \t\n")
        throw ();

        /**
         * Removes trailing stuff. (Left trim.)
         *
         * @param[in] string the string to trim
         * @param[in] chars the characters to remove (default: white space)
         * @return the trimmed string
         */
        static std::string ltrim(const std::string &string,
                                const char *chars = " \t\n")
        throw();

        /**
         * Removes leading stuff. (Left trim.)
         *
         * @param[in] string the string to trim
         * @param[in] chars the characters to remove (default: white space)
         * @return the trimmed string
         */
        static std::string rtrim(const std::string &string,
                                const char *chars = " \t\n")
        throw();

        /**
         * Converts a string vector to a string.
         *
         * @param[in] begin start of the sequence
         * @param[in] end end of the sequence
         *
         * @return the string
         */
        static std::string vector2string(const StringVector &vector,
                                         const char *delimiter = " ")
        throw ();

        /**
         * Splits lines.
         *
         * @param[in] string a multiline string
         * @return the lines (with "\n" removed)
         */
        static StringVector splitlines(const std::string &string)
        throw ();

        /**
         * Checks if @p long_string starts with @p part.
         *
         * @param[in] long_string the long string
         * @param[in] part the string part
         * @return @c true if @p long_string starts with @p part, @c false
         *         otherwise
         */
        static bool startsWith(const std::string &long_string,
                               const std::string &part)
        throw ();

};

//}}}
//{{{ Stringutil implementation ------------------------------------------------

// -----------------------------------------------------------------------------
template <typename numeric_type>
std::string Stringutil::number2string(numeric_type number)
    throw ()
{
    std::stringstream ss;
    ss << number;
    return ss.str();
}

//}}}
#endif /* STRINGUTIL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

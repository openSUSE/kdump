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
#include <iomanip>

#include "global.h"

#define ISO_DATETIME "%Y-%m-%d-%H:%M"
#define MD5SUM_LENGTH	16
#define SHA1SUM_LENGTH	20

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
         * Transforms a numberic string value into a string in hex format
         * The string includes the '0x' prefix.
         *
         * @param[in] number the number to transform
         * @return the string, e.g. <tt>0x1234</tt>
         */
        template <typename numeric_type>
        static std::string number2hex(numeric_type number)
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
         * Parses a number.
         *
         * @param[in] string string that should be parsed
         * @return the number
         */
        static long long string2llong(const std::string &string)
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
         * Splits a string.
         *
         * @param[in] string a string that may contain or may contain not
         *            the split character
         * @param[in] split the split character
         * @return the vector of split strings
         */
        static StringVector split(const std::string &string, char split)
        throw ();
        
        /**
         * Joins a string vector into a string.
         *
         * @param[in] stringvector a vector of strings
         * @param[in] joinchar the character between the elements
         * @return the resulting string
         */
        static std::string join(const StringVector &stringvector,
                                char joinchar)
        throw ();
         

        /**
         * Formats the current time as specified in @p formatstring.
         *
         * @param[in] formatstring strftime(3)-like format string
         * @return the formatted output
         */
        static std::string formatCurrentTime(const char *formatstring)
        throw ();

        /**
         * Formats the given Unix time as specified in @p formatstring.
         *
         * @param[in] formatstring strftime(3)-like format string
         * @param[in] Unix time stamp
         * @return the formatted output
         */
        static std::string formatUnixTime(const char *formatstring,
                                          time_t value)
        throw ();

#if HAVE_LIBSSL

	/**
	 * Computes the MD5 and SHA1 digests of a buffer.
	 *
	 * @param[in] buf base-64 encoded data to be digested
	 * @param[in] len length of the base64 input
	 * @param[out] md5sum resulting MD5 sum
	 * @param[out] sha1sum resulting SHA1 sum
	 */
	static void digest_base64(const void *buf, size_t len,
				  char *md5sum, char *sha1sum)
	throw (KError);

#endif	// HAVE_LIBSSL

	static int hex2int(char c)
	throw (KError);
};

//}}}
//{{{ KString implementation ---------------------------------------------------

/**
 * Enhancements of class std::string.
 */
class KString : public std::string {
    public:
        /**
         * Standard constructors (refer to std::string).
         */
        KString()
        : std::string()
        {}
        KString(const std::string& str)
        : std::string(str)
        {}
        KString(const std::string& str, size_type pos, size_type len = npos)
        : std::string(str, pos, len)
        {}
        KString(const char* s)
        : std::string(s)
        {}
        KString(const char* s, size_type n)
        : std::string(s, n)
        {}
        KString(size_type n, char c)
        : std::string(n, c)
        {}
        template <class InputIterator>
            KString(InputIterator first, InputIterator last)
        : std::string(first, last)
        {}

        /**
         * Checks if the string is an integer number ('+' or '-' is allowed).
         *
         * @return true if it's an number, false otherwise
         */
        bool isNumber()
        throw ();

        /**
         * Checks if the string is a hexadecimal number (no leading '0x').
         *
         * @return true if it's a hex number, false otherwise
         */
        bool isHexNumber()
        throw ();

        /**
         * Remove trailing or leading stuff.
         *
         * @param[in] chars the characters to remove (default: white space)
         * @return reference to this instance
         */
        KString& trim(const char *chars = " \t\n")
        throw ();

        /**
         * Removes trailing stuff. (Left trim.)
         *
         * @param[in] chars the characters to remove (default: white space)
         * @return reference to this instance
         */
        KString& ltrim(const char *chars = " \t\n")
        throw();

        /**
         * Removes leading stuff. (Left trim.)
         *
         * @param[in] chars the characters to remove (default: white space)
         * @return the trimmed string
         */
        KString& rtrim(const char *chars = " \t\n")
        throw();

        /**
         * Checks if the string starts with @p part.
         *
         * @param[in] part the string part
         * @return @c true if string starts with @p part, @c false otherwise
         */
        bool startsWith(const std::string &part) const
        throw ();

        /**
         * Checks if the string ends with @p part.
         *
         * @param[in] part the string part
         * @return @c true if string ends with @p part, @c false otherwise
         */
        bool endsWith(const std::string &part) const
        throw ();

	/**
	 * Perform URL decoding on the string.
	 *
	 * @param[in] formenc if @c true, translate '+' into spaces
	 * @return reference to this object (after decoding)
	 */
	KString &decodeURL(bool formenc = false)
	throw();
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

// -----------------------------------------------------------------------------
template <typename numeric_type>
std::string Stringutil::number2hex(numeric_type number)
    throw ()
{
    std::stringstream ss;
    ss << "0x";
    ss << std::hex << number;
    return ss.str();
}

//}}}
#endif /* STRINGUTIL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

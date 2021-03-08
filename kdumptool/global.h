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
#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdexcept>
#include <list>
#include <string>
#include <vector>
#include <map>

#include "config.h"

//{{{ Constants ----------------------------------------------------------------

#define DEFAULT_DUMP        "/proc/vmcore"
#define PATH_SEPARATOR      "/"

//}}}
//{{{ Type definitions ---------------------------------------------------------

typedef std::list<std::string> StringList;
typedef std::vector<unsigned char> ByteVector;
typedef std::map<std::string, std::string> StringStringMap;

//}}}
//{{{ Macros -------------------------------------------------------------------

/**
 * Some protection against NULL strings for functions that cannot deal with
 * NULL. Returns "(null)" if the string is NULL and the string itself otherwise.
 */
#define SAVE_CHARSTRING(x) \
    (x) ? (x) : ("null")

//}}}
//{{{ KErrorCode ---------------------------------------------------------------

/**
 * Pure virtual class which serves as a base class for errors described
 * by integer code values.
 */
class KErrorCode {
    public:
        KErrorCode(int code)
            : m_code(code)
        {}

        virtual std::string message(void) const = 0;

        int getCode(void) const
        { return m_code; }

    private:
        int m_code;
};

//}}}
//{{{ KError -------------------------------------------------------------------

/**
 * Standard error class.
 */
class KError : public std::runtime_error {
    public:
        /**
         * Creates a new object of KError with string as error message.
         *
         * @param string the error message
         */
        KError(const std::string& string)
            : std::runtime_error(string) {}

};

//}}}
//{{{ KCodeError ---------------------------------------------------------------

/**
 * Standard error class template for errors that have a numeric code.
 */
template <class ErrorCode>
class KCodeError : public KError {
    public:
        /**
         * Creates a new object of KError with an error message format:
         *
         *   'message (ErrorCode(errorcode).message())'
         *
         * @param message the error message
         * @param errorcode the system error code (errno)
         */
        KCodeError(const std::string& message, int errorcode)
            : KError(message + " (" + ErrorCode(errorcode).message() + ")")
        {}
};

//}}}
//{{{ KSystemErrorCode ---------------------------------------------------------

/**
 * Class for errors that store a value in the errno variable.
 */
class KSystemErrorCode : public KErrorCode {
    public:
        KSystemErrorCode(int code)
            : KErrorCode(code)
        {}

        virtual std::string message(void) const;
};

//}}}
//{{{ KGaiErrorCode ------------------------------------------------------------

/**
 * Class for errors returned by getaddrinfo().
 */
class KGaiErrorCode : public KErrorCode {
    public:
        KGaiErrorCode(int code)
            : KErrorCode(code)
        {}

        virtual std::string message(void) const;
};

//}}}
//{{{ Pre-defined error classes ------------------------------------------------

typedef KCodeError<KSystemErrorCode> KSystemError;
typedef KCodeError<KGaiErrorCode> KGaiError;

//}}}

#endif /* GLOBAL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

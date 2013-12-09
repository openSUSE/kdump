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
#define LINE                "---------------------------------------" \
                            "---------------------------------------"
#define GLOBALDEBUGDIR      "/usr/lib/debug"
#define PATH_SEPARATOR      "/"

//}}}
//{{{ Type definitions ---------------------------------------------------------

typedef std::list<std::string> StringList;
typedef std::vector<std::string> StringVector;
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

class KErrorCode {
    public:
        KErrorCode(int code)
	    : m_code(code)
        {}

        virtual std::string message(void) const
	throw () = 0;

        int getCode(void) const
        throw ()
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
//{{{ KSystemError -------------------------------------------------------------

/**
 * Standard error class for system errors that have a valid errno information.
 */
class KSystemError : public KError {
    public:
        /**
         * Creates a new object of KError with an error message format:
         *
         *   'message (strerror(errorcode))'
         *
         * @param message the error message
         * @param errorcode the system error code (errno)
         */
        KSystemError(const std::string& message, int errorcode);
};

//}}}
//{{{ KNetError ----------------------------------------------------------------

/**
 * Standard error class for network errors that store the error information
 * in a variable called h_errno.
 */
class KNetError : public KError {
    public:

        /**
         * Creates a new object of KError with an error message format:
         *
         *   'message (hstrerror(errorcode))'
         *
         * @param[in] message the error message
         * @param[in] errorcode the value of h_errno
         */
        KNetError(const std::string &message, int errorcode);
};

//}}}
//{{{ KSFTPErrorCode -----------------------------------------------------------

class KSFTPErrorCode : public KErrorCode {
    public:
        KSFTPErrorCode(int code)
	    : KErrorCode(code)
        {}

        virtual std::string message(void) const
	throw ();
};

//}}}
//{{{ KSFTPError ---------------------------------------------------------------

/**
 * Standard error class for SFTP (libssh2) errors.
 */
class KSFTPError : public KError {
    public:

        /**
         * Creates a new object of KSFTPError with an error message format:
         *
         *   'message (KSFTPCode(errorcode).message())'
         *
         * @param[in] string the error message
         * @param[in] errorcode the value of the sftp_sftp_get_last_error()
         */
        KSFTPError(const std::string &message, int errorcode);
};

//}}}
//{{{ KELFError ----------------------------------------------------------------

/**
 * Standard error class for libelf error.
 */
class KELFError : public KError {
    public:

        /**
         * Creates a new object of KError with an error message format:
         *
         *   'message (elf_errmsg(errorcode))'
         *
         * @param[in] message the error message
         * @param[in] errorcode the value of elf_errno()
         */
        KELFError(const std::string &message, int errorcode);
};

//}}}
//{{{ KSmtpErrorCode -----------------------------------------------------------

class KSmtpErrorCode : public KErrorCode {
    public:
        KSmtpErrorCode(int code)
	    : KErrorCode(code)
        {}

        virtual std::string message(void) const
	throw ();
};

//}}}
//{{{ KSmtpError ---------------------------------------------------------------

/**
 * Standard error class for libelf error.
 */
class KSmtpError : public KError {
    public:

        /**
         * Creates a new object of KError with an error message format:
         *
         *   'message (KSmtpCode(errorcode).message())'
         *
         * @param[in] message the error message
         * @param[in] errorcode the value of elf_errno()
         */
        KSmtpError(const std::string &message, int errorcode);
};

//}}}

#endif /* GLOBAL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

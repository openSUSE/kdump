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


//{{{ Constants ----------------------------------------------------------------

#define DEFAULT_DUMP        "/proc/vmcore"
#define LINE                "---------------------------------------" \
                            "---------------------------------------"
#define GLOBALDEBUGDIR      "/usr/lib/debug"

//}}}
//{{{ Type definitions ---------------------------------------------------------

typedef std::list<std::string> StringList;
typedef std::vector<std::string> StringVector;
typedef std::vector<unsigned char> ByteVector;
typedef std::map<std::string, std::string> StringStringMap;

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
         * Creates a new object of KError with string as error message.
         *
         * @param string the error message
         * @param errorcode the system error code (errno)
         */
        KSystemError(const std::string& string, int errorcode)
            : KError(string), m_errorcode(errorcode),
              m_errorstring(string) {}

        /**
         * Returns a readable error message from the string and the error
         * code.
         *
         * @return Error message in the format 'string (strerror(errorcode))'.
         */
        virtual const char *what() const
        throw ();

        /**
         * Don't know why that is necessary to avoid compiler errors.
         */
        virtual ~KSystemError() throw () {}

    private:
        int m_errorcode;
        std::string m_errorstring;
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
         * Creates a new object of KError with string as error message.
         *
         * @param[in] string the error message
         * @param[in] errorcode the value of h_errno
         */
        KNetError(const std::string &string, int errorcode)
            : KError(string), m_errorcode(errorcode),
              m_errorstring(string) {}

        /**
         * Returns a readable error message from the string and the error code.
         *
         * @return Error message in the format 'string (strerror(errorcode))'.
         */
        virtual const char *what() const
        throw ();

        /**
         * Don't know why that is necessary to avoid compiler errors.
         */
        virtual ~KNetError() throw () {}

    private:
        int m_errorcode;
        std::string m_errorstring;
};

//}}}
//{{{ KSFTPError ---------------------------------------------------------------

/**
 * Standard error class for SFTP (libssh2) errors.
 */
class KSFTPError : public KError {
    public:

        /**
         * Creates a new object of KSFTPError with string as error message.
         *
         * @param[in] string the error message
         * @param[in] errorcode the value of the sftp_sftp_get_last_error()
         */
        KSFTPError(const std::string &string, int errorcode)
            : KError(string), m_errorcode(errorcode),
              m_errorstring(string) {}

        /**
         * Returns a readable error message from the string and the error code.
         *
         * @return Error message in the format 'string (strerror(errorcode))'.
         */
        virtual const char *what() const
        throw ();

        /**
         * Don't know why that is necessary to avoid compiler errors.
         */
        virtual ~KSFTPError() throw () {}

    protected:
        std::string getStringForCode(int code) const
        throw ();

    private:
        int m_errorcode;
        std::string m_errorstring;
};

//}}}
//{{{ KELFError ----------------------------------------------------------------

/**
 * Standard error class for libelf error.
 */
class KELFError : public KError {
    public:

        /**
         * Creates a new object of KELFError with string as error message.
         *
         * @param[in] string the error message
         * @param[in] errorcode the value of elf_errno()
         */
        KELFError(const std::string &string, int errorcode)
            : KError(string), m_errorcode(errorcode),
              m_errorstring(string) {}

        /**
         * Returns a readable error message from the string and the error code.
         *
         * @return Error message in the format 'string (strerror(errorcode))'.
         */
        virtual const char *what() const
        throw ();

        /**
         * Don't know why that is necessary to avoid compiler errors.
         */
        virtual ~KELFError() throw () {}

    private:
        int m_errorcode;
        std::string m_errorstring;
};

//}}}

#endif /* GLOBAL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

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

//{{{ Type definitions ---------------------------------------------------------

typedef std::list<std::string> StringList;
typedef std::vector<std::string> StringVector;
typedef std::vector<unsigned char> ByteVector;

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

#endif /* GLOBAL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

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
#ifndef UTIL_H
#define UTIL_H

#include <zlib.h>

#include "subcommand.h"
#include "global.h"

//{{{ Util ---------------------------------------------------------------------

/**
 * Various utility functions.
 */
class Util {

    public:
        /**
         * Returns the architecture.
         */
        static std::string getArch()
        throw (KError);

        /**
         * Checks if the given architecture is x86.
         */
        static bool isX86(const std::string &arch)
        throw ();

        /**
         * Checks if the specified file is a gzip compressed file.
         *
         * @param file the name of the file
         */
        static bool isGzipFile(const char *file)
        throw (KError);

        /**
         * Frees a vector.
         */
        template <typename T>
        static void freev(T **vector)
        throw ();

        /**
         * Makes the current process a daemon running in the background.
         *
         * @exception KError if something goes wrong
         */
        static void daemonize()
        throw (KError);

        /**
         * Checks if the buffer is entirely zero.
         *
         * @param[in] buffer the buffer to check
         * @param[in] size the size of the buffer
         * @return @c true if all bytes are zero, @c false if not
         */
        static bool isZero(const char *buffer, size_t size)
        throw ();
};

//}}}
//{{{ Template implementations -------------------------------------------------
template <typename T>
void Util::freev(T **vector)
    throw ()
{
    T **p = vector;

    while (*p) {
        delete[] *p;
        p++;
    }

    delete[] vector;
}

#endif /* UTIL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

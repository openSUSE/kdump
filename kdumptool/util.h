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
        static std::string getArch();

        /**
         * Returns the kernel release number (uname -r) of the kernel that
         * is currently active.
         *
         * @return the kernel release
         */
        static std::string getKernelRelease();

        /**
         * Checks if the given architecture is x86.
         */
        static bool isX86(const std::string &arch);

        /**
         * Checks if the specified file is a gzip compressed file.
         *
         * @param[in] fd a file descriptor
         * @return @c true if the file has the gzip header, @c false otherwise
         * @exception KError if opening the file failed
         */
        static bool isGzipFile(int fd);

        /**
         * Checks if the specified file is a gzip compressed file.
         *
         * @param file the name of the file
         * @return @c true if the file has the gzip header, @c false otherwise
         * @exception KError if opening the file failed
         */
        static bool isGzipFile(const std::string &file);

        /**
         * Checks if @p filename is an ELF file.
         *
         * @param[in] filename the file to check
         * @return @c true if it's an ELF file, @c false otherwise
         * @exception KError if opening the file failed
         */
        static bool isElfFile(const std::string &filename);

        /**
         * Checks if @p filename is an ELF file.
         *
         * @param[in] fd a file descriptor
         * @return @c true if it's an ELF file, @c false otherwise
         * @exception KError if opening the file failed
         */
        static bool isElfFile(int fd);

        /**
         * Checks if @p filename is a Xen core dump.
         *
         * @param[in] filename the file to check
         * @return @c true if it's a Xen core dump, @c false otherwise
         * @exception KError if opening the file failed
         */
        static bool isXenCoreDump(const std::string &filename);

        /**
         * Checks if @p filename is a Xen core dump.
         *
         * @param[in] fd a file descriptor
         * @return @c true if it's a Xen core dump, @c false otherwise
         * @exception KError if opening the file failed
         */
        static bool isXenCoreDump(int fd);

        /**
         * Makes the current process a daemon running in the background.
         *
         * @exception KError if something goes wrong
         */
        static void daemonize();

        /**
         * Checks if the buffer is entirely zero.
         *
         * @param[in] buffer the buffer to check
         * @param[in] size the size of the buffer
         * @return @c true if all bytes are zero, @c false if not
         */
        static bool isZero(const char *buffer, size_t size);

        /**
         * Returns the system hostname and domainname in the form
         * hostname.domainname.
         *
         * @return the hostname.domainname pair
         * @exception KError if providing the hostname and domainname failed
         */
        static std::string getHostDomain();

        /**
         * Finds a byte sequence in another byte sequence.
         *
         * @param[in] haystack the buffer in which will be searched
         * @param[in] haystack_len the length of @p haystack
         * @param[in] needle the buffer to search for
         * @param[in] needle_len the length of the @p needle buffer
         * @return the offset in @p haystack or -1 if @p needle was not found
         *         in @p haystack
         */
        static ssize_t findBytes(const unsigned char *haystack,
				 size_t haystack_len,
				 const unsigned char *needle,
				 size_t needle_len);

        /**
         * Retrieves the environment variables @p env.
         *
         * @param[in] env the environment variable to retrieve
         * @param[in] defaultValue the default which will be returned
         *            if @p env is not set
         * @param[out] isDefault when non-NULL, will be set to @c true
         *             if @p defaultValue will be returned to discriminate
         *             the case when the actual value is the default value
         * @return the environment value of @p env or @p default if there
         *         is no such environment
         */
        static std::string getenv(const std::string &env,
                                  const std::string &defaultValue,
                                  bool *isDefault = NULL);
};

//}}}

#endif /* UTIL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

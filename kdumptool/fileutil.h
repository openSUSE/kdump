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
#ifndef FILEUTIL_H
#define FILEUTIL_H


//{{{ Transfer -----------------------------------------------------------------

/**
 * Various file utility functions.
 */
class FileUtil {

    public:

        /**
         * Creates a new directory.
         *
         * @param[in] dir the name of the directory that should be created
         * @param[in] recursive @b true if the behaviour of <tt>mkdir -p</tt>
         *            should be copied, @c false otherwise.
         *
         * @throw KError on any error
         */
        static void mkdir(const std::string &dir, bool recursive)
        throw (KError);

        /**
         * Checks if the specified file exists.
         *
         * @param[in] file the file that should be checked
         * @return @c true on success, @c false otherwise
         */
        static bool exists(const std::string &file)
        throw ();

        /**
         * Gets the base name of a file. This function is not thread-safe!
         *
         * @param[in] path the path for which the base name should be provided
         * @return the base name
         */
        static std::string basename(const std::string &file)
        throw ();

        /**
         * Gets the directory name of a file. This function is not thread-safe!
         *
         * @param[in] path the path for which the directory name should be
         *            provided
         * @return the directory name
         */
        static std::string dirname(const std::string &file)
        throw ();

        /**
         * Concatenates two path components.
         *
         * @param[in] a the first path component
         * @param[in] b the second path component
         * @return a + "/" + b
         */
        static std::string pathconcat(const std::string &a,
                                      const std::string &b)
        throw ();
};

//}}}

#endif /* FILEUTIL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

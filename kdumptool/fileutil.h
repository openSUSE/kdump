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

#include "global.h"
#include "stringutil.h"

//{{{ FileUtil -----------------------------------------------------------------

/**
 * Various file utility functions.
 */
class FileUtil {

    public:

        /**
         * Get the current working directory.
         *
         * @throw KError on any error
         */
        static std::string getcwd(void)
        throw (KError);

        /**
         * Mounts a file system to a given mountpoint.
         *
         * @param[in] device the device or a CIFS share or a NFS target
         *            (like //hostname/sharename for CIFS or hostname:directory
         *            for NFS
         * @param[in] mountpoint the local path where the file system should
         *            be mounted
         * @param[in] fs the file system (can be empty)
         * @param[in] options a list of options (without the "-o")
         *
         * @exception KError if the mount did not succeed
         */
         static void mount(const std::string &device,
                           const std::string &mountpoint,
                           const std::string &fs,
                           const StringVector &options)
         throw (KError);

         /**
          * Mounts a NFS directory.
          *
          * @param[in] host remote host
          * @param[in] dir directory to mount
          * @param[in] mountpoint the local path where the file system should
          *            be mounted
          * @param[in] options a list of mount options (without the "-o")
          * @param[out] mountdir the directory that is actually used to
          *             mount the @p dir (i.e. a subset of @p dir)
          *
          * @exception KError if the mount did not succeed.
          */
         static void nfsmount(const std::string &host,
                               const std::string &dir,
                               const std::string &mountpoint,
                               const StringVector &options)
         throw (KError);

         /**
          * Unmounts a file system from the given mountpoint.
          *
          * @param[in] mountpoint the mount point
          *
          * @exception KError if the mount did not succeed.
          */
         static void umount(const std::string &device)
         throw (KError);

         /**
          * Gets the sorted list of the contents of the specified directory.
          * The contents is sorted alphabetically, and "." and ".." entries
          * are omitted.
          *
          * @param[in] dir the directory to list
          * @param[in] @c true if only dirs should be included in the directory
          *            list, @c false if all files should be included
          * @exception KError if something went wrong
          */
         static StringVector listdir(const std::string &dir, bool onlyDirs)
         throw (KError);
};

//}}}
//{{{ FilePath -----------------------------------------------------------------

/**
 * File path.
 */
class FilePath : public KString {

    private:
        static const std::string m_slash;

    public:
        /**
         * Standard constructors (refer to std::string).
         */
        FilePath()
        : KString()
        {}
        FilePath(const std::string& str)
        : KString(str)
        {}
        FilePath(const std::string& str, size_type pos, size_type len = npos)
        : KString(str, pos, len)
        {}
        FilePath(const char* s)
        : KString(s)
        {}
        FilePath(const char* s, size_type n)
        : KString(s, n)
        {}
        FilePath(size_type n, char c)
        : KString(n, c)
        {}
        template <class InputIterator>
            FilePath(InputIterator first, InputIterator last)
        : KString(first, last)
        {}

        /**
         * Gets the base name of a file. This function is not thread-safe!
         *
         * @return the base name
         */
        std::string baseName() const
        throw ();

        /**
         * Gets the directory name of a file. This function is not thread-safe!
         *
         * @return the directory name
         */
        std::string dirName() const
        throw ();

        /**
         * Concatenates two path components.
         *
         * @param[in] p the path component to be appended
         * @return reference to this instance
         */
        FilePath& appendPath(const std::string &p)
        throw ();

        /**
         * Checks if the specified file exists.
         *
         * @return @c true on success, @c false otherwise
         */
        bool exists() const
        throw ();

        /**
         * Checks if the given path is a symbolic link.
         *
         * @throw KError on any error
         */
        bool isSymlink() const
        throw (KError);

        /**
         * Reads a symbolic link. Does the same like readlink(2), only
         * that it's C++ and an exception is thrown on error instead of
         * giving an error code.
         *
         * @return the resolved link
         *
         * @throw KError if an error occured
         */
        std::string readLink() const
        throw (KError);

        /**
         * Returns the canonical representation of the specified path.
         * This means that all symbolic links are resolved. It does that
         * as if the root directory was @p root.
         *
         * @param[in] root the new root where the function should chroot to
         * @return the canonical representation of the path
         *
         * @throw KError when chroot or realpath() fail
         */
        FilePath getCanonicalPath(const std::string &root = m_slash) const
        throw (KError);

        /**
         * Returns the size of a given file.
         *
         * @return the size of the file
         * @exception KError if the file does not exist or if stat() does
         *            fail for another reason
         */
        unsigned long long fileSize() const
        throw (KError);

        /**
         * Get the free disk size in bytes.
         *
         * @return the free disk size in bytes
         * @exception KError if the underlying statfs() call fails
         */
        unsigned long long freeDiskSize() const
        throw (KError);

        /**
         * Creates a new directory in the specified path.
         *
         * @param[in] recursive @b true if the behaviour of <tt>mkdir -p</tt>
         *            should be copied, @c false otherwise.
         *
         * @throw KError on any error
         */
        void mkdir(bool recursive)
        throw (KError);

        /**
         * Delete the specified directory.
         *
         * @param[in] recursive @c true if all contents of non-empty
         *            directories should be deleted, @c false otherwise
         * @exception KError if something went wrong
         */
        void rmdir(bool recursive)
        throw (KError);

};

//}}}
//{{{ Functions ----------------------------------------------------------------

/**
 * Converts bytes to megabytes.
 *
 * @param[in] bytes the bytes
 * @return megabytes
 */
static inline unsigned long long bytes_to_megabytes(unsigned long long bytes)
{
    return bytes / 1024 / 1024;
}

/**
 * Converts bytes to kilobytes.
 *
 * @param[in] bytes the bytes
 * @return kilobytes
 */
static inline unsigned long long bytes_to_kilobytes(unsigned long long bytes)
{
    return bytes / 1024;
}

//}}}

#endif /* FILEUTIL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

/*
 * (c) 2021, Petr Tesarik <ptesarik@suse.de>, SUSE Linux Software Solutions GmbH
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

#ifndef KERNELPATH_H
#define KERNELPATH_H

#include <string>

#include "fileutil.h"
#include "stringutil.h"

//{{{ KernelPath --------------------------------------------------------------

/**
 * Kernel image path.
 *
 * The path is split into the directory name and the kernel version.
 * For example, if you make a KernelPath object for
 * "/boot/vmlinuz-5.3.18-lp152.60-default", then the version is
 * "5.3.18-lp152.60-default". If the file name is a plain image name
 * (e.g. "vmlinuz"), then the version is empty.
 */
class KernelPath {
    private:
        FilePath m_directory;
        KString m_name;
        KString m_version;

    public:
        KernelPath(FilePath const &path);

        /**
         * Returns possible image names for the specific architecture
         * in the order they should be tried.
         *
         * @param[in] arch the architecture name such as "i386"
         * @return a list of suitable image names like ("vmlinuz", "vmlinux")
         */
        static StringList imageNames(const std::string &arch);

        /**
         * @return path to the kernel directory
         */
        const FilePath& directory()
        { return m_directory; }

        /**
         * @return kernel image name
         */
        const KString& name()
        { return m_name; }

        /**
         * @return kernel version (may be empty)
         */
        const KString& version()
        { return m_version; }

        /**
         * Constructs the corresponding config file path.
         *
         * @return path to the config (may not exist)
         */
        FilePath configPath();

        /**
         * Constructs the corresponding initrd path.
         *
         * @param[in] fadump are we making initrd for FADUMP?
         * @return the path to the initrd (may not exist)
         */
        FilePath initrdPath(bool fadump);

        /**
         * Checks if the path refers to a kdump kernel flavour.
         *
         * @return @c true if the path is a kdump kernel, @c false otherwise
         */
        bool isKdump();
};

//}}}

#endif /* KERNELPATH_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

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
#ifndef KERNELTOOL_H
#define KERNELTOOL_H

#include <string>

#include "global.h"

//{{{ KernelTool ---------------------------------------------------------------

/**
 * Helper functions to work with kernel images.
 */
class KernelTool {

    public:
        /**
         * Type of the kernel.
         */
        enum KernelType {
            KT_ELF,
            KT_ELF_GZ,
            KT_X86,
            KT_NONE
        };

    public:

        /**
         * Returns the type for a specified kernel image.
         *
         * @param[in] file the file of the kernel image
         * @exception KError if opening of the kernel image fails
         */
        static KernelType getKernelType(const std::string &file)
        throw (KError);

        /**
         * Checks if a x86 kernel image is relocatable.
         *
         * @param[in] file the full path to the kernel image. The kernel must
         *            be a bzImage of x86.
         * @return @c true if the kernel is relocatable, @c false otherwise
         */
        static bool x86isRelocatable(const std::string &file)
        throw (KError);

    private:
        static bool isX86Kernel(const std::string &file)
        throw (KError);
};

//}}}

#endif /* IDENTIFYKERNEL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

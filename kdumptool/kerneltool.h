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
         * Creates a new kerneltool object.
         *
         * @param[in] the kernel image to get information for
         * @throw KError if opening of the kernel failed
         */
        KernelTool(const std::string &image)
        throw (KError);
        
        /**
         * Destroys a kerneltool object.
         */
        virtual ~KernelTool();

        /**
         * Returns the type for the kernel image specified in the constructor.
         *
         * @exception KError if opening of the kernel image fails
         */
        KernelType getKernelType() const
        throw (KError);

        /**
         * Checks if a kernel is relocatable, regardless of the kernel type.
         *
         * @return @c true if the kernel is relocatable, @c false otherwise
         */
        bool isRelocatable() const
        throw (KError);

    protected:
        /**
         * Checks if a x86 kernel image is relocatable.
         *
         * @return @c true if the kernel is relocatable, @c false otherwise
         */
        bool x86isRelocatable() const
        throw (KError);

        /**
         * Checks if the architecture has always relocatable kernels.
         *
         * @param[in] machine the machine, e.g. "ia64"
         * @return @c true if it always has relocatable architectures,
         *         @c false otherwise
         */
        bool isArchAlwaysRelocatable(const std::string &machine) const
        throw ();

        /**
         * Checks if a ELF kernel image is relocatable.
         *
         * @return @c true if the ELF kernel is relocatable, @c false otherwise
         */
        bool elfIsRelocatable() const
        throw (KError);

        /**
         * Checks if the kernel is a x86 kernel.
         *
         * @return @c true if it's a x86 kernel, @c false otherwise.
         */
        bool isX86Kernel() const
        throw (KError);

        /**
         * Returns the architecture as string from the ELF machine type.
         *
         * @param[in] et_machine the ELF machine type such as EM_386
         * @return the architecture as string such as "i386".
         */
        std::string archFromElfMachine(unsigned long long et_machine) const
        throw ();
        
    private:
        int m_fd;
};

//}}}

#endif /* IDENTIFYKERNEL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

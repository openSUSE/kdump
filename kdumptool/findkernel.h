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
#ifndef FINDKERNEL_H
#define FINDKERNEL_H

#include "stringutil.h"
#include "fileutil.h"
#include "subcommand.h"

//{{{ FindKernel ---------------------------------------------------------------

/**
 * Subcommand to find a suitable kernel on the system.
 */
class FindKernel : public Subcommand {

    public:
        /**
         * Creates a new FindKernel object.
         */
        FindKernel()
        throw ();

    public:
        /**
         * Returns the name of the subcommand (find_kernel).
         */
        const char *getName() const
        throw ();

        /**
         * Executes the function.
         *
         * @throw KError on any error. No exception indicates success.
         */
        void execute()
        throw (KError);

    protected:
        
        /**
         * Checks if a given kernel image is suitable for kdump.
         *
         * @param[in] kernelImage ful path to the kernel image
         * @param[in] strict if that parameter is set to @c true, then
         *            we consider kernels that are not well suited as
         *            capture kernel (such as RT or huge number of CPUs
         *            because they waste memory) as unsuited. For @c false
         *            we just check if the kernel can be loaded at all.
         * @return @c true if the kernel is suited (see also @p strict) and
         *         @c false otherwise
         * @exception KError if opening of @p kernelImage fails or reading
         *            of the kernel configuration of @p kernelImage fails
         */
        bool suitableForKdump(const std::string &kernelImage, bool strict)
        throw (KError);
        
        /**
         * Checks if the given kernel image is a kdump kernel. Currently
         * only name matching is done.
         *
         * @param[in] kernelimage the full path of the kernel image
         * @return @c true if the image is a kernel image, @c false otherwise
         * @exception KError on any error
         */
        bool isKdumpKernel(const KString &kernelimage)
        throw (KError);

        /**
         * Finds a kernel image that fits to the specified kernel version.
         * It looks in /boot.
         *
         * @param[in] kernelver the kernel version to look for
         * @return the kernel image or an empty string if there's no such kernel
         *         image
         * @exception KError on any error
         */
        std::string findForVersion(const std::string &kernelver)
        throw (KError);
        
        /**
         * Automatically finds a suitable kdump kernel. See kdump(5) for
         * documentation which kernel is taken.
         *
         * @return the full path to the kernel image
         * @exception KError on any error
         */
        std::string findKernelAuto()
        throw (KError);
        
        /**
         * Finds an initrd for the given kernel image.
         *
         * @param[in] kernelPath a full path to a kernel image
         * @return the initrd or an empty string if there's no initrd
         */
        std::string findInitrd(const FilePath &kernelPath)
        throw ();

    private:
        bool m_checkRelocatable;
        bool m_checkType;
        std::string m_kernelImage;
        std::string m_arch;
};

//}}}

#endif /* FINDKERNEL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

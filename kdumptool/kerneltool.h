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

class Kconfig;

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
            KT_S390,
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
         * Returns possible image names for the specific architecture
         * in the order we should try them.
         *
         * @param[in] arch the architecture name such as "i386"
         * @return a list of suitable image names like ("vmlinuz", "vmlinux")
         */
        static std::list<std::string> imageNames(const std::string &arch)
        throw ();

        /**
         * Uses KernelTool::imageNames() and strips off the image names
         * from kernelImage. For example, pass /boot/vmlinuz-2.6.27-rc1,
         * then you get "/boot" as @p directory and "2.6.27-rc1" as @p rest.
         *
         * @param[in] kernelImage the full path to the kernel image
         * @param[out] directory the directory where @p kernelImage was in
         * @param[out] rest the rest, see the description above
         * @return @c true if something has been stripped off, @c false
         *         otherwise. The result is valid in any case.
         * @exception KError if something went wrong
         */
        static bool stripImageName(const std::string &kernelImage,
                                   std::string &directory,
                                   std::string &rest)
        throw (KError);

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
        
        /**
         * Extracts the kernel configuration from a kernel image. The kernel
         * image can be of type ELF, ELF.gz and bzImage.
         *
         * @return the embedded kernel configuration
         * @exception KError if reading of the kernel image failed
         */
        std::string extractKernelConfig() const
        throw (KError);
        
        /**
         * Retrieves a Kconfig object for the given kernel.
         *
         * The difference between extractKernelConfig() is following:
         * That function actually parses the kernel configuration extracted
         * by extractKernelConfig(). It also checks first if we have
         * a valid configuration file on disk, that means if the kernel
         * is /boot/vmlinuz-2.6.27-pae then it checks for
         * /boot/config-2.6.27-pae. That is just faster than getting the
         * kernel configuration from the image.
         *
         * If it is not possible to read the configuration (from the file and/or
         * for the kernel image), then a KError is thrown.
         *
         * @return a Kconfig object that has to be freed by the caller
         *         (it's a pointer to avoid a cyclic dependency between
         *         Kconfig and KernelTool)
         */
        Kconfig *retrieveKernelConfig() const
        throw (KError);

        /**
         * String representation of kerneltool.
         *
         * @return "[KernelTool] name"
         */
        std::string toString() const
        throw ();

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
         * Checks if the kernel is a S/390 kernel image.
         *
         * @return @c true if it's a S/390 kernel image, @c false otherwise.
         */
        bool isS390Kernel() const
        throw (KError);

        /**
         * Returns the architecture as string from the ELF machine type.
         *
         * @param[in] et_machine the ELF machine type such as EM_386
         * @return the architecture as string such as "i386".
         */
        std::string archFromElfMachine(unsigned long long et_machine) const
        throw ();
        
                /**
         * Extracts the kernel configuration from a ELF kernel image.
         * The image may be compressed.
         *
         * @param[in] kernelImage full path to the kernel image
         * @return the embedded kernel configuration
         * @exception KError if reading of the kernel image failed
         */
        std::string extractKernelConfigELF() const
        throw (KError);

        /**
         * Extracts the kernel configuration from a bzImage. The
         * image may be compressed.
         *
         * @param[in] image full path to the kernel image
         * @return the kernel configuration
         * @exception KError if reading of the kernel image failed
         */
        std::string extractKernelConfigbzImage() const
        throw (KError);

        /**
         * Extracts the kernel configuration from a IKCONFIG buffer.
         *
         * @param[in] buffer the buffer with the data bytes
         * @param[in] buflen the size of the buffer
         * @return the configuration string
         * @exception KError if the buffer contains invalid data
         */
        std::string extractFromIKconfigBuffer(const char *buffer, size_t buflen)
        const throw (KError);

    private:
        std::string m_kernel;
        int m_fd;
};

//}}}

#endif /* IDENTIFYKERNEL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

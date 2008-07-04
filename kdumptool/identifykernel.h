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
#ifndef IDENTIFYKERNEL_H
#define IDENTIFYKERNEL_H

#include "subcommand.h"

//{{{ IdentifyKernel -----------------------------------------------------------

/**
 * Subcommand to identify a kernel binary.
 * The source is original from a C source file, so it's not really C++'ish.
 */
class IdentifyKernel : public Subcommand {

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

        /**
         * Error codes for IdentifyKernel. The numeric values are for
         * compatibility.
         */
        enum ErrorCode {
            SUCCESS,
            NOT_RELOCATABLE = 2,
            NOT_A_KERNEL = 3
        };

    public:
        /**
         * Creates a new IdentifyKernel object.
         */
        IdentifyKernel()
        throw ();

    public:
        /**
         * Returns the name of the subcommand (identify_kernel).
         */
        const char *getName() const
        throw ();

        /**
         * Returns the list of options supported by this subcommand.
         */
        OptionList getOptions() const
        throw ();

        /**
         * Parses the command line options.
         */
        void parseCommandline(OptionParser *optionparser)
        throw (KError);

        /**
         * Executes the function.
         *
         * @throw KError on any error. No exception indicates success.
         */
        void execute()
        throw (KError);

    protected:
        bool isElfFile(const char *filename)
        throw (KError);

        bool checkElfFile(const char *file)
        throw (KError);

        bool checkArchFile(const char *file)
        throw (KError);

        bool isX86Kernel(const char *file)
        throw (KError);

        bool checkArchFileX86(const char *file)
        throw (KError);

        bool isArchAlwaysRelocatable(const char *machine)
        throw ();

        KernelType getKernelType(const char *file)
        throw (KError);

    private:
        bool m_checkRelocatable;
        bool m_checkType;
        std::string m_kernelImage;
};

//}}}

#endif /* IDENTIFYKERNEL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

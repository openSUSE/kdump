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

//{{{ IdentifyKernelCommand ----------------------------------------------------

/**
 * Subcommand to identify a kernel binary.
 * The source is original from a C source file, so it's not really C++'ish.
 */
class IdentifyKernel : public Subcommand {

    public:

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
        IdentifyKernel();

    public:
        /**
         * Returns the name of the subcommand (identify_kernel).
         */
        const char *getName() const;

        /**
         * Parses the non-option arguments from the command line.
         */
        virtual void parseArgs(const StringVector &args);

        /**
         * Executes the function.
         *
         * @throw KError on any error. No exception indicates success.
         */
        void execute();

    protected:
        bool checkElfFile(const char *file);

        bool checkArchFile(const char *file);

        bool checkArchFileX86(const char *file);

        bool isArchAlwaysRelocatable(const char *machine);

    private:
        bool m_checkRelocatable;
        bool m_checkType;
        std::string m_kernelImage;
};

//}}}

#endif /* IDENTIFYKERNEL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

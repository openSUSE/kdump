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
#ifndef DEBUGLINK_H
#define DEBUGLINK_H

#include <string>
#include <stdint.h>

#include <libelf.h>
#include <gelf.h>

#include "global.h"

//{{{ Debuglink ----------------------------------------------------------------

/**
 * Get the .gnu_debuglink.
 */
class Debuglink {

    public:

        /**
         * Creates a new Debuglink object.
         *
         * @param[in] file the name of the file. Can be ELF or ELF.gz.
         * @exception KError if the file does not exist.
         */
        Debuglink(const char *file)
        throw (KError);

        /**
         * Destroys a Debuglink object.
         */
        ~Debuglink()
        throw () {}

        /**
         * Reads the debug link from the file.
         *
         * @exception KError if reading the file fails or if the format is
         *            not supported
         */
        void readDebuglink()
        throw (KError);

        /**
         * Finds the debuginfo file on the system.
         *
         * @param[in] prefix add that prefix to global path names
         * @return the path to the debuginfo file
         * @exception KError if something went wrong
         */
        std::string findDebugfile(const char *prefix="")
        throw (KError);

        /**
         * Returns the name of the debuglink. You must have called the
         * Debuglink::readDebuglink() method before.
         *
         * @exception KError if Debuglink::readDebuglink() has not been called.
         */
        std::string getDebuglink() const
        throw (KError);

        /**
         * Returns the CRC32 checksum.
         *
         * @return the CRC32 checksum
         * @exception KError if Debuglink::readDebuglink() has not been called.
         */
        uint32_t getCrc() const
        throw (KError);

    protected:
        int openCompressed()
        throw (KError);

        int openUncompressed()
        throw (KError);

        Elf *openElf(int fd)
        throw (KError);

        void readDebuginfoLink(Elf *elf)
        throw (KError);

        uint32_t calcCrc(const std::string &file)
        throw (KError);


    private:
        std::string m_filename;
        std::string m_debuglink;
        uint32_t m_crc;
};

//}}}

#endif /* DEBUGLINK_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

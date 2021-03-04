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
#ifndef VMCOREINFO_H
#define VMCOREINFO_H

#include <iostream>
#include <ctime>

#include "global.h"

//{{{ Vmcoreinfo ---------------------------------------------------------------

/**
 * Represents the VMCOREINFO of makedumpfile.
 */
class Vmcoreinfo {

    public:

        /**
         * Creates a new Vmcoreinfo object.
         *
         * @exception KError if the libelf library is out of date
         */
        Vmcoreinfo();

        /**
         * Deletes the Vmcoreinfo object.
         */
        virtual ~Vmcoreinfo()
        { }

        /**
         * Reads the vmcoreinfo from a ELF file as NOTES section.
         *
         * @param[in] elf_file the ELF file
         * @exception KError if reading the vmcoreinfo failed
         */
        void readFromELF(const char *elf_file);

        /**
         * Gets all keys.
         *
         * @return a list of keys
         */
        StringList getKeys() const;

        /**
         * Returns a value.
         *
         * @param[in] the key
         * @return the value for @p key
         * @exception KError if the value has not been found
         */
        std::string getStringValue(const char *key) const;

        /**
         * Returns an integer value.
         *
         * @param[in] the key
         * @return the value for @p key
         * @exception KError if the value has not been found
         */
        int getIntValue(const char *key) const;

        /**
         * Returns an integer value.
         *
         * @param[in] the key
         * @return the value for @p key
         * @exception KError if the value has not been found
         */
        int getLLongValue(const char *key) const;

        /**
         * Returns true if the VMCOREINFO is VMCOREINFO_XEN, and false
         * otherwise.
         *
         * @return @c true if it's @c VMCOREINFO_XEN and false otherwise.
         */
        bool isXenVmcoreinfo() const;

    protected:
        ByteVector readElfNote(const char *file);

        ByteVector readVmcoreinfoFromNotes(const char *buffer, size_t size,
                                           bool isElf64);

    private:
        StringStringMap m_map;
        bool m_xenVmcoreinfo;
};

//}}}

#endif /* VMCOREINFO_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

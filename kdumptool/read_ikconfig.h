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
#ifndef READ_IKCONFIG_H
#define READ_IKCONFIG_H

#include "subcommand.h"

//{{{ ReadIKConfig -------------------------------------------------------------

/**
 * Subcommand to read the embedded kernel configuration.
 */
class ReadIKConfig : public Subcommand {

    public:
        /**
         * Creates a new ReadIKConfig object.
         */
        ReadIKConfig();

    public:

        /**
         * Returns the name of the subcommand (read_ikconfig).
         */
        const char *getName() const;

        /**
         * Parses the non-option arguments from the command line.
         */
        virtual void parseArgs(const StringVector &args);

        /**
         * That command does not need a config file.
         */
        bool needsConfigfile() const;

        /**
         * Executes the function.
         *
         * @throw KError on any error. No exception indicates success.
         */
        void execute();

    private:
        std::string m_file;
};

//}}}

#endif /* READ_IKCONFIG_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

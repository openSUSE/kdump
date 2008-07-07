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
#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "global.h"
#include "optionparser.h"
#include "subcommand.h"

//{{{ Configuration ------------------------------------------------------------

/**
 * Configuration. This is a singleton object. To use it, call
 *
 *   Configuration::config()->readConfig(filename, variables)
 *
 * once. If you didn't call this, you'll get a KError on every attempt to
 * read a value.
 *
 * This configuration parser is shell based. This means, it sources the
 * configuration in a shell (/bin/sh), and prints the evaluated value from
 * that shell. This output is parsed.
 *
 * This mechanism is necessary for the /etc/sysconfig files to be parsed
 * according to the standard.
 */
class Configuration {

    public:
        /**
         * Returns the only configuration object.
         *
         * @return the configuration object
         */
        static Configuration *config()
        throw ();

        /**
         * Reads a configuration file.
         *
         * @param filename the file name to read
         * @param variables a list of variables where to read
         * @exception if the @c filename was not found or the shell that
         *            is necessary to parse the configuration file could not
         *            be spawned
         */
        void readFile(const std::string &filename,
            const StringVector &variables)
        throw (KError);

    protected:
        virtual ~Configuration()
        throw () {}

    private:
        static Configuration *m_instance;
        StringVector m_variables;
};

//}}}

#endif /* CONFIGURATION_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

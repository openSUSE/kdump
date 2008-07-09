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
#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

#include <string>

#include "global.h"
#include "optionparser.h"
#include "subcommand.h"

//{{{ ConfigParser -------------------------------------------------------------

/**
 * Configuration parser.
 *
 * This configuration parser is shell based. This means, it sources the
 * configuration in a shell (/bin/sh), and prints the evaluated value from
 * that shell. This output is parsed.
 *
 * This mechanism is necessary for the /etc/sysconfig files to be parsed
 * according to the standard.
 *
 * To use it
 *
 *  - create a new object with the file name specified in the constructor,
 *  - add variables to the configuration parser that should be parsed
 *  - parse the file with the @c ConfigParser::parse() method.
 *  - and retrieve the variables using the different get methods.
 *
 * This class implements a read-only parser. You cannot write configuration
 * files with that implementation.
 */
class ConfigParser {

    public:

        /**
         * Creates a new ConfigParser object with the specified file name
         * as configuration file.
         *
         * @param[in] filename the file name of the configuration file
         *            (here it is not checked if the file exists)
         */
        ConfigParser(const std::string &filename)
        throw ();

        /**
         * Deletes the ConfigParser object.
         */
        virtual ~ConfigParser() throw ()
        {}

        /**
         * Adds a variable which should be parsed.
         *
         * @param[in] name the name of the variable
         */
        void addVariable(const std::string &name)
        throw ();

        /**
         * Parse the configuration file.
         *
         * @exception KError if opening of the file failed or if spawning the
         *            shell that actually parses the configuration file fails
         */
        void parse()
        throw (KError);

        /**
         * Returns the value of the specified configuration option.
         *
         * @param[in] name the configuration option (case matters)
         * @return the value
         *
         * @exception KError if the value cannot be found
         */
        std::string getValue(const std::string &name) const
        throw (KError);

        /**
         * Returns the value of an integer configuration option. The parser
         * tries to convert the string value which can be obtained by
         * ConfigParser::getValue() to an integer.
         *
         * @param[in] name the configuration option (case matters)
         * @return the value as integer
         *
         * @exception KError if the value cannot be found
         */
        int getIntValue(const std::string &name) const
        throw (KError);

        /**
         * Returns the value of a boolean configuration option. The parser
         * tries to convert the string value which can be obtained by
         * ConfigParser::getValue() to a boolean value.
         *
         * @param[in] name the configuration option (case matters)
         * @return the value as boolean
         *
         * @exception KError if the value cannot be found
         */
        bool getBoolValue(const std::string &name) const
        throw (KError);

    private:
        std::string m_configFile;
        StringStringMap m_variables;
};

//}}}

#endif /* CONFIGPARSER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

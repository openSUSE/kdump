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

//{{{ ConfigParser -------------------------------------------------------------

/**
 * Configuration parser.
 *
 * To use it
 *
 *  - create a new object with the file name specified in the constructor,
 *  - add the variables to be parsed to the configuration parser
 *  - parse the file with the @c ConfigParser::parse() method.
 *  - and retrieve the variable values
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
        throw ()
	: m_configFile(filename)
	{}

        /**
         * Deletes the ConfigParser object.
         */
        virtual ~ConfigParser()
	throw ()
        {}

        /**
         * Adds a variable which should be parsed.
         *
         * @param[in] name the name of the variable
	 * @param[in] defvalue default value for the variable
         */
        void addVariable(const std::string &name, const std::string &defvalue)
        throw ();

        /**
         * Parse the configuration file.
         *
         * @exception KError if opening of the file failed or if spawning the
         *            shell that actually parses the configuration file fails
         */
        virtual void parse()
        throw (KError) = 0;

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

    protected:
        std::string m_configFile;
        StringStringMap m_variables;
};

//}}}

//{{{ ShellConfigParser --------------------------------------------------------

/**
 * This configuration parser is shell based. This means, it sources the
 * configuration in a shell (/bin/sh), and prints the evaluated value from
 * that shell. This output is parsed.
 *
 * This mechanism is necessary for the /etc/sysconfig files to be parsed
 * according to the standard.
 */
class ShellConfigParser : public ConfigParser {

    public:

        /**
         * Creates a new ShellConfigParser object with the specified file
         * name as configuration file.
         *
         * @param[in] filename the file name of the configuration file
         *            (here it is not checked if the file exists)
         */
        ShellConfigParser(const std::string &filename)
        throw ();

        /**
         * Parse the configuration file.
         *
         * @exception KError if opening of the file failed or if spawning the
         *            shell that actually parses the configuration file fails
         */
        virtual void parse()
        throw (KError);
};

//}}}

//{{{ KernelConfigParser -------------------------------------------------------

/**
 * This configuration parser parses the kernel command line. It reads a file
 * (like /proc/cmdline) and applies a reimplementation of the algorithm used
 * by the Linux kernel.
 */
class KernelConfigParser : public ConfigParser {

    public:

        /**
         * Creates a new KernelConfigParser object with the specified file
         * name as configuration file.
         *
         * @param[in] filename the file name of the configuration file
         *            (here it is not checked if the file exists)
         */
        KernelConfigParser(const std::string &filename)
        throw ();

        /**
         * Parse the configuration file.
         *
         * @exception KError if opening of the file failed
         */
        virtual void parse()
        throw (KError);
};

//}}}

#endif /* CONFIGPARSER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

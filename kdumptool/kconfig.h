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
#ifndef KCONFIG_H
#define KCONFIG_H

#include <iostream>
#include <ctime>

#include "global.h"
#include "kerneltool.h"

//{{{ KconfigValue -------------------------------------------------------------

/**
 * Represents a value in the kernel configuration. As values can be multiple
 * types, we need an own class for that.
 *
 * KconfigValue objects cannot change the value. You have to create one from
 * KconfigValue::fromString() when parsing the kernel configuration since that
 * method also has access to private data.
 */
class KconfigValue {

    public:

        /**
         * Type of a configuration option.
         */
        enum Type {
            T_INVALID,      /**< invalid values such as comments returned by
                                 fromString() */
            T_STRING,       /**< string value such as CONFIG_ARCH_DEFCONFIG */
            T_INTEGER,      /**< integer value such as CONFIG_LOG_BUF_SHIFT */
            T_TRISTATE      /**< tristate value such as CONFIG_MARKERS */
        };

        /**
         * Tristate value.
         */
        enum Tristate {
            ON,             /**< CONFIG=y */
            OFF,            /**< # CONFIG is not set */
            MODULE          /**< CONFIG=m */
        };

    public:

        /**
         * Creates a invalid configuration option (T_INVALID).
         */
        KconfigValue();

        /**
         * Returns a KconfigValue value from a .config line. If the line is
         * an empty line or an comments (except that "is not set"), we return
         * a value object whose type is T_INVALID.
         *
         * @param[in] string a line. It does not matter if the trailing newline
         *            is present, that will be stripped anyway.
         * @param[out] name will be set to the name of the configuration option
         *             if and only if the type of the return value is not
         *             T_INVALID.
         * @return a new KconfigValue object
         * @exception Kconfig if the line cannot be from a .config file
         */
        static KconfigValue fromString(const std::string &string,
                                       std::string &name);

        /**
         * Returns the type of the KconfigValue.
         *
         * @return the type
         */
        enum Type getType() const;

        /**
         * For T_STRING objects, return the string without quotation marks.
         *
         * @return the string value
         * @exception KError if the type is not T_STRING
         */
        std::string getStringValue() const;

        /**
         * For T_INTEGER objects, return the integer value.
         *
         * @return the integer value
         * @exception KError if the type is not T_INTEGER.
         */
        int getIntValue() const;

        /**
         * For T_TRISTATE objects, return the tristate value.
         *
         * @return the tristate value, see enum Tristate.
         * @exception KError if the type is not T_INTEGER.
         */
        enum Tristate getTristateValue() const;

        /**
         * Converts a KconfigValue into a string representation.
         *
         * @return the string representation
         */
        std::string toString() const;

    private:
        enum Type m_type;
        std::string m_string;
        int m_integer;
        enum Tristate m_tristate;
};

/**
 * Prints a KconfigValue with standard C++ operators.
 *
 * @param[in,out] os the stream to print
 * @param[in] v the value to print to
 * @return the @p os
 */
std::ostream& operator<<(std::ostream& os, const KconfigValue& v);

//}}}
//{{{ Kconfig ------------------------------------------------------------------

/**
 * Represents the kernel configuration (.config).
 */
class Kconfig {

    public:

        /**
         * Deletes the Vmcoreinfo object.
         */
        virtual ~Kconfig()
        { }

        /**
         * Reads the kernel configuration from a normal (like
         * /boot/config-2.6.27-pae) or a  .gz config file (like
         * /proc/config.gz).
         *
         * @param[in] configFile the full path to the configuration file
         * @exception KError if reading of the configuration file fails
         */
        void readFromConfig(const std::string &configFile);

        /**
         * Reads the configuration from a kernel image if the kernel has been
         * compiled with CONFIG_IKCONFIG.
         *
         * @param[in] kernelImage the kernel image to look for
         * @exception KError if reading of the kernel image fails
         */
        void readFromKernel(const std::string &kernelImage);

        /**
         * Reads the configuration from a kernel image if the kernel has been
         * compiled with CONFIG_IKCONFIG.
         *
         * @param[in] kt a kerneltool object
         * @exception KError if reading of the kernel image fails
         */
        void readFromKernel(const KernelTool &kt);

        /**
         * Returns the configuration value for a specific option.
         *
         * @param[in] optionName the name of the option, must be @b with the
         *            "CONFIG_" prefix. It's also required to write the
         *            configuration name in capital letters, i.e. exactly as
         *            in the .config file.
         * @return the configuration option value, the function returns
         *         a KconfigValue with type T_INVALID.
         */
        KconfigValue get(const std::string &option);

    private:
        std::map<std::string, KconfigValue> m_configs;
};

//}}}

#endif /* KCONFIG_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

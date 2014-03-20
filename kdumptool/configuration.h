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

#include <vector>
#include <typeinfo>

#include "global.h"

//{{{ ConfigOption -------------------------------------------------------------
/**
 * Generic configuration option.
 */
class ConfigOption {

    public:
        /**
	 * These flags determine at which stage the option is used.
	 */
	enum UsageFlags {
	    USE_MKINITRD,	// Used when generating initrd
	    USE_KEXEC,		// Used when loading the dump kernel
	    USE_DUMP,		// Used for dumping
	    USE_MAX		// Max usage bit position
	};

    public:
	ConfigOption(const char *name, unsigned usage)
	throw ()
	: m_name(name), m_usage(usage)
	{ }

	/**
	 * Return the name of the option.
	 */
	const char *name() const
	throw ()
	{ return m_name; }

	/**
	 * Return the usage flags of the option.
	 */
	unsigned usage() const
	throw ()
	{ return m_usage; }

	/**
	 * Return the string representation of the value.
	 */
	virtual std::string valueAsString() const = 0;

	/**
	 * Update the value from a parser.
	 *
	 * @param val new value of the option (as a string)
	 */
	virtual void update(const std::string &value)
	throw (KError) = 0;

	/**
	 * Return true if this option is assigned the default value.
	 */
	virtual bool isDefault(void)
	throw () = 0;

    protected:
	const char *const m_name;
	const unsigned m_usage;
};

//}}}

//{{{ StringConfigOption -------------------------------------------------------
/**
 * String configuration option.
 */
class StringConfigOption : public ConfigOption {

    public:
	StringConfigOption(const char *name, unsigned usage,
			   const char *const defvalue)
	throw ()
	: ConfigOption(name, usage), m_defvalue(defvalue), m_value(defvalue)
	{ }

	/**
	 * Get the config option value.
	 */
	const std::string &value(void) const
	throw ()
	{ return m_value; }

	/**
	 * Return the string representation of the value.
	 * This is the same as StringConfigOption::value(), but it
	 * returns a copy of the string rather than a constant reference.
	 */
	virtual std::string valueAsString() const
	throw ();

	/**
	 * Update the value from a parser.
	 *
	 * @param val new value of the option (as a string)
	 */
	virtual void update(const std::string &value)
	throw (KError);

	/**
	 * Return true if this option is assigned the default value.
	 */
	virtual bool isDefault(void)
	throw ()
	{ return m_value == m_defvalue; }

    protected:
	const char *const m_defvalue;
	std::string m_value;
};

//}}}

//{{{ IntConfigOption ----------------------------------------------------------
/**
 * Integer configuration option.
 */
class IntConfigOption : public ConfigOption {

    public:
	IntConfigOption(const char *name, unsigned usage, const int defvalue)
	throw ()
	: ConfigOption(name, usage), m_defvalue(defvalue), m_value(defvalue)
	{ }

	/**
	 * Get the config option value.
	 */
	int value(void) const
	throw ()
	{ return m_value; }

	/**
	 * Return the string representation of the value.
	 */
	virtual std::string valueAsString() const
	throw ();

	/**
	 * Update the value from a parser.
	 *
	 * @param val new value of the option (as a string)
	 */
	virtual void update(const std::string &value)
	throw (KError);

	/**
	 * Return true if this option is assigned the default value.
	 */
	virtual bool isDefault(void)
	throw ()
	{ return m_value == m_defvalue; }

    protected:
	const int m_defvalue;
	int m_value;
};

//}}}

//{{{ BoolConfigOption ---------------------------------------------------------
/**
 * Boolean configuration option.
 */
class BoolConfigOption : public ConfigOption {

    public:
	BoolConfigOption(const char *name, unsigned usage, const bool defvalue)
	throw ()
	: ConfigOption(name, usage), m_defvalue(defvalue), m_value(defvalue)
	{ }

	/**
	 * Get the config option value.
	 */
	bool value(void) const
	throw ()
	{ return m_value; }

	/**
	 * Return the string representation of the value.
	 */
	virtual std::string valueAsString() const
	throw ();

	/**
	 * Update the value from a parser.
	 *
	 * @param val new value of the option (as a string)
	 */
	virtual void update(const std::string &value)
	throw (KError);

	/**
	 * Return true if this option is assigned the default value.
	 */
	virtual bool isDefault(void)
	throw ()
	{ return m_value == m_defvalue; }

    protected:
	const bool m_defvalue;
	bool m_value;
};

//}}}

//{{{ Configuration ------------------------------------------------------------

typedef std::vector<ConfigOption*>::const_iterator ConfigOptionIterator;

/**
 * Configuration. This is a singleton object. To use it, call
 *
 *   Configuration::config()->readConfig(filename)
 *
 * once. If you didn't call this, you'll get a KError on every attempt to
 * read a value.
 *
 * The actual parsing is done in the ConfigParser class.
 */
class Configuration {

    public:
        /**
         * Verbosity flags.
         */
        enum VerbosityFlags {
             VERB_LOG_CMDLINE       = (1<<0),
             VERB_PROGRESS          = (1<<1),
             VERB_STDOUT_CMDLINE    = (1<<2),
             VERB_DEBUG_TRANSFER    = (1<<3)
        };

        /**
         * Configuration options.
         */
#define DEFINE_OPT(name, type, defval, usage)		\
        type ## ConfigOption name;
#include "define_opt.h"
#undef DEFINE_OPT

        /**
         * Total number of configuration options.
         */
        static const int optionCount = sizeof(
/* Since the include file does not contain anything beyond tokens removed
 * or interpreted by the preprocessor, we can use string concatenation to
 * create a dummy string with as many characters as there are options.
 */
#define DEFINE_OPT(name, type, defval, usage) "."
#include "define_opt.h"
#undef DEFINE_OPT
            ) - 1;           /* for the NUL character */

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
         * @exception if the @c filename was not found or the shell that
         *            is necessary to parse the configuration file could not
         *            be spawned
         */
        void readFile(const std::string &filename)
        throw (KError);

        /**
         * Reads a kernel command file.
         *
         * @param filename the file (e.g. /proc/cmdline)
         * @exception if the @c filename was not found
         */
        void readCmdLine(const std::string &filename)
        throw (KError);

        /**
	 * Checks if KDUMPTOOL_FLAGS contains @p flag.
	 *
	 * @return @c true if KDUMPTOOL_FLAGS contains the flag and @c false
	 *         otherwise
	 * @exception see Configuration::getStringValue()
	 */
	bool kdumptoolContainsFlag(const std::string &flag)
	throw (KError, std::out_of_range, std::bad_cast);

	/*
	 * Checks whether this configuration needs network.
	 *
	 * @return @c true if target is non-local, or mail is configured,
	 *         @c false otherwise
	 *
	 */
	bool needsNetwork();

	/*
	 * Checks whether this configuration needs makedumpfile.
	 *
	 * @return @c false if vmcore can be simply copied, and @c false
	 *         if makedumpfile is needed to do the filtering
	 */
	bool needsMakedumpfile();

	ConfigOptionIterator optionsBegin() const
	throw ()
	{ return m_options.begin(); }

	ConfigOptionIterator optionsEnd() const
	throw ()
	{ return m_options.end(); }

    protected:
        Configuration()
        throw ();

        virtual ~Configuration()
        throw () {}

    private:
        static Configuration *m_instance;
        bool m_readConfig;

	std::vector<ConfigOption*> m_options;
};

//}}}

#endif /* CONFIGURATION_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

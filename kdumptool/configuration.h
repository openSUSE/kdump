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
#include "optionparser.h"
#include "configparser.h"
#include "subcommand.h"

//{{{ ConfigOption -------------------------------------------------------------
/**
 * Generic configuration option.
 */
class ConfigOption {

    public:
	ConfigOption(const char *name)
	throw ()
	: m_name(name)
	{ }

	/**
	 * Register the configuration option with a parser.
	 *
	 * @cp   the ConfigParser object where the config option will
	 *       be registered.
	 */
	void registerVar(ConfigParser &cp) const
	throw ()
	{
	    cp.addVariable(m_name);
	}

	/**
	 * Update the value from a parser.
	 *
	 * @cp   the ConfigParser object from which the value will be
	 *       obtained.
	 */
	virtual void update(ConfigParser &cp)
	throw (KError) = 0;

    protected:
	const char *const m_name;
};

//}}}

//{{{ StringConfigOption -------------------------------------------------------
/**
 * String configuration option.
 */
class StringConfigOption : public ConfigOption {

    public:
	StringConfigOption(const char *name, const char *const defvalue)
	throw ()
	: ConfigOption(name), m_defvalue(defvalue), m_value(defvalue)
	{ }

	/**
	 * Get the config option value.
	 */
	const std::string &value(void) const
	throw ()
	{ return m_value; }

	/**
	 * Update the value from a parser.
	 *
	 * @cp   the ConfigParser object from which the value will be
	 *       obtained.
	 */
	virtual void update(ConfigParser &cp)
	throw (KError);

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
	IntConfigOption(const char *name, const int defvalue)
	throw ()
	: ConfigOption(name), m_defvalue(defvalue), m_value(defvalue)
	{ }

	/**
	 * Get the config option value.
	 */
	int value(void) const
	throw ()
	{ return m_value; }

	/**
	 * Update the value from a parser.
	 *
	 * @cp   the ConfigParser object from which the value will be
	 *       obtained.
	 */
	virtual void update(ConfigParser &cp)
	throw (KError);

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
	BoolConfigOption(const char *name, const bool defvalue)
	throw ()
	: ConfigOption(name), m_defvalue(defvalue), m_value(defvalue)
	{ }

	/**
	 * Get the config option value.
	 */
	bool value(void) const
	throw ()
	{ return m_value; }

	/**
	 * Update the value from a parser.
	 *
	 * @cp   the ConfigParser object from which the value will be
	 *       obtained.
	 */
	virtual void update(ConfigParser &cp)
	throw (KError);

    protected:
	const bool m_defvalue;
	bool m_value;
};

//}}}

//{{{ Configuration ------------------------------------------------------------

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
	 * Configuration option index.
	 */
	enum OptionIndex {
#define DEFINE_OPT(name, type, defval) \
	    name,
#include "define_opt.h"
#undef DEFINE_OPT
	};

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
        void readFile(const std::string &filename)
        throw (KError);

	/**
	 * Return the value of a string option.
	 *
	 * @param index the option index
	 * @return the option value
	 * @exception std::bad_cast if the option index is not a string
	 *            or an exception thrown by Configuration::getOptionPtr()
	 */
	const std::string &getStringValue(enum OptionIndex index) const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return dynamic_cast<StringConfigOption&>
		(*getOptionPtr(index)).value();
	}

	/**
	 * Return the value of an integer option.
	 *
	 * @param index the option index
	 * @return the option value
	 * @exception std::bad_cast if the option index is not an int
	 *            or an exception thrown by Configuration::getOptionPtr()
	 */
	int getIntValue(enum OptionIndex index) const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return dynamic_cast<IntConfigOption&>
		(*getOptionPtr(index)).value();
	}

	/**
	 * Return the value of a boolean option.
	 *
	 * @param index the option index
	 * @return the option value
	 * @exception std::bad_cast if the option index is not a boolean
	 *            or an exception thrown by Configuration::getOptionPtr()
	 */
	bool getBoolValue(enum OptionIndex index) const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return dynamic_cast<BoolConfigOption&>
		(*getOptionPtr(index)).value();
	}

	/**
         * Returns the value of KDUMP_KERNELVER.
         *
         * @return the kernel version
	 * @exception see Configuration::getStringValue()
         */
        std::string getKernelVersion() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(KDUMP_KERNELVER);
	}

	/**
	 * Returns the value of KDUMP_CPUS.
	 *
	 * @return the desired parallelism
	 * @exception see Configuration::getIntValue()
	 */
	int getCPUs() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getIntValue(KDUMP_CPUS);
	}

	/**
	 * Returns the value of MAKEDUMPFILE_OPTIONS.
	 *
	 * @return the options for makedumpfile
	 * @exception see Configuration::getStringValue()
	 */
	std::string getMakedumpfileOptions() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(MAKEDUMPFILE_OPTIONS);
	}

	/**
	 * Returns the value of KDUMP_SAVEDIR.
	 *
	 * @return the URL where the dump should be saved to
	 * @exception see Configuration::getStringValue()
	 */
	std::string getSavedir() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(KDUMP_SAVEDIR);
	}

	/**
	 * Returns the value of KDUMP_KEEP_OLD_DUMPS.
	 *
	 * @return the number of old dumps that should be kept
	 * @exception see Configuration::getIntValue()
	 */
	int getKeepOldDumps() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getIntValue(KDUMP_KEEP_OLD_DUMPS);
	}

        /**
	 * Returns the value of KDUMP_FREE_DISK_SIZE.
	 *
	 * @return the disk size in megabytes that should stay free
	 * @exception see Configuration::getIntValue()
	 */
	int getFreeDiskSize() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getIntValue(KDUMP_FREE_DISK_SIZE);
	}

        /**
	 * Returns the value of KDUMP_VERBOSE.
	 *
	 * @return a bit mask that represents the verbosity
	 * @exception see Configuration::getIntValue()
	 */
	int getVerbose() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getIntValue(KDUMP_VERBOSE);
	}

        /**
	 * Returns the value of KDUMP_DUMPLEVEL.
	 *
	 * @return the dump level (for makedumpfile)
	 * @exception see Configuration::getIntValue()
	 */
	int getDumpLevel() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getIntValue(KDUMP_DUMPLEVEL);
	}

        /**
	 * Returns the value of KDUMP_DUMPFORMAT.
	 *
	 * @return the dump format (@c ELF, @c compressed, @c "")
	 * @exception see Configuration::getStringValue()
	 */
	std::string getDumpFormat() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(KDUMP_DUMPFORMAT);
	}

        /**
	 * Returns the value of KDUMP_CONTINUE_ON_ERROR.
	 *
	 * @return @c true if kdump should continue on error, @c false
	 *         otherwise
	 * @exception see Configuration::getBoolValue()
	 */
	bool getContinueOnError() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getBoolValue(KDUMP_CONTINUE_ON_ERROR);
	}

        /**
	 * Returns the value of KDUMP_COPY_KERNEL.
	 *
	 * @return @c true if the full kernel should be copied, @c false
	 *         otherwise
	 * @exception see Configuration::getBoolValue()
	 */
	bool getCopyKernel() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getBoolValue(KDUMP_COPY_KERNEL);
	}

        /**
	 * Returns the value of KDUMPTOOL_FLAGS.
	 *
	 * @return the flags
	 * @exception see Configuration::getStringValue()
	 */
	std::string getKdumptoolFlags() const
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(KDUMPTOOL_FLAGS);
	}

        /**
	 * Checks if KDUMPTOOL_FLAGS contains @p flag.
	 *
	 * @return @c true if KDUMPTOOL_FLAGS contains the flag and @c false
	 *         otherwise
	 * @exception see Configuration::getStringValue()
	 */
	bool kdumptoolContainsFlag(const std::string &flag)
	throw (KError, std::out_of_range, std::bad_cast);

        /**
	 * Returns KDUMP_SMTP_SERVER.
	 *
	 * @return the STMP server or "" if no SMTP server has been
	 *         specified in the configuration file
	 * @exception see Configuration::getStringValue()
	 */
	std::string getSmtpServer()
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(KDUMP_SMTP_SERVER);
	}

	/**
	 * Returns the SMTP username if SMTP AUTH is used. That's the value
	 * of KDUMP_SMTP_USER.
	 *
	 * @return the SMTP user name
	 * @exception see Configuration::getStringValue()
	 */
        std::string getSmtpUser()
        throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(KDUMP_SMTP_USER);
	}

	/**
	 * Returns the SMTP password if SMTP AUTH is used. That's the value
	 * of KDUMP_SMTP_PASSWORD.
	 *
	 * @return the STMP password
	 * @exception see Configuration::getStringValue()
	 */
	std::string getSmtpPassword()
        throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(KDUMP_SMTP_PASSWORD);
	}

	/**
	 * Returns the value of KDUMP_NOTIFICATION_TO.
	 *
	 * @return the notification mail address
	 * @exception see Configuration::getStringValue()
	 */
	std::string getNotificationTo()
        throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(KDUMP_NOTIFICATION_TO);
	}

	/**
	 * Returns the value of KDUMP_NOTIFICATION_CC.
	 *
	 * @return the notification Cc address
	 * @exception see Configuration::getStringValue()
	 */
	std::string getNotificationCc()
        throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(KDUMP_NOTIFICATION_CC);
	}

        /**
	 * Returns the value of KDUMP_HOST_KEY.
	 *
	 * @return the target host key, encoded with base64
	 * @exception see Configuration::getStringValue()
	 */
	std::string getHostKey()
	throw (KError, std::out_of_range, std::bad_cast)
	{
	    return getStringValue(KDUMP_HOST_KEY);
	}

    protected:
        Configuration()
        throw ();

        virtual ~Configuration()
        throw () {}

	/**
	 * Get a pointer to a raw configuration option.
	 *
	 * @param index The option index.
	 * @exception KError if the configuration has not been read yet
	 *            std::out_of_range if an invalid index is used
	 */
	ConfigOption *getOptionPtr(enum OptionIndex index) const
	throw (KError, std::out_of_range);

    private:
        static Configuration *m_instance;
        bool m_readConfig;

	std::vector<ConfigOption*> m_options;
};

//}}}

#endif /* CONFIGURATION_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

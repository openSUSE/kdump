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
         * Returns the value of KDUMP_KERNELVER.
         *
         * @return the kernel version
	 * @exception see Configuration::getOption

         */
        std::string getKernelVersion() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(KDUMP_KERNELVER))->value();
	}

	/**
	 * Returns the value of KDUMP_CPUS.
	 *
	 * @return the desired parallelism
	 * @exception see Configuration::getOption
	 */
	int getCPUs() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<IntConfigOption*>
		(getOption(KDUMP_CPUS))->value();
	}

	/**
	 * Returns the value of MAKEDUMPFILE_OPTIONS.
	 *
	 * @return the options for makedumpfile
	 * @exception see Configuration::getOption
	 */
	std::string getMakedumpfileOptions() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(MAKEDUMPFILE_OPTIONS))->value();
	}

	/**
	 * Returns the value of KDUMP_SAVEDIR.
	 *
	 * @return the URL where the dump should be saved to
	 * @exception see Configuration::getOption
	 */
	std::string getSavedir() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(KDUMP_SAVEDIR))->value();
	}

	/**
	 * Returns the value of KDUMP_KEEP_OLD_DUMPS.
	 *
	 * @return the number of old dumps that should be kept
	 * @exception see Configuration::getOption
	 */
	int getKeepOldDumps() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<IntConfigOption*>
		(getOption(KDUMP_KEEP_OLD_DUMPS))->value();
	}

        /**
	 * Returns the value of KDUMP_FREE_DISK_SIZE.
	 *
	 * @return the disk size in megabytes that should stay free
	 * @exception see Configuration::getOption
	 */
	int getFreeDiskSize() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<IntConfigOption*>
		(getOption(KDUMP_FREE_DISK_SIZE))->value();
	}

        /**
	 * Returns the value of KDUMP_VERBOSE.
	 *
	 * @return a bit mask that represents the verbosity
	 * @exception see Configuration::getOption
	 */
	int getVerbose() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<IntConfigOption*>
		(getOption(KDUMP_VERBOSE))->value();
	}

        /**
	 * Returns the value of KDUMP_DUMPLEVEL.
	 *
	 * @return the dump level (for makedumpfile)
	 * @exception see Configuration::getOption
	 */
	int getDumpLevel() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<IntConfigOption*>
		(getOption(KDUMP_DUMPLEVEL))->value();
	}

        /**
	 * Returns the value of KDUMP_DUMPFORMAT.
	 *
	 * @return the dump format (@c ELF, @c compressed, @c "")
	 * @exception see Configuration::getOption
	 */
	std::string getDumpFormat() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(KDUMP_DUMPFORMAT))->value();
	}

        /**
	 * Returns the value of KDUMP_CONTINUE_ON_ERROR.
	 *
	 * @return @c true if kdump should continue on error, @c false
	 *         otherwise
	 * @exception see Configuration::getOption
	 */
	bool getContinueOnError() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<BoolConfigOption*>
		(getOption(KDUMP_CONTINUE_ON_ERROR))->value();
	}

        /**
	 * Returns the value of KDUMP_COPY_KERNEL.
	 *
	 * @return @c true if the full kernel should be copied, @c false
	 *         otherwise
	 * @exception see Configuration::getOption
	 */
	bool getCopyKernel() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<BoolConfigOption*>
		(getOption(KDUMP_COPY_KERNEL))->value();
	}

        /**
	 * Returns the value of KDUMPTOOL_FLAGS.
	 *
	 * @return the flags
	 * @exception see Configuration::getOption
	 */
	std::string getKdumptoolFlags() const
        throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(KDUMPTOOL_FLAGS))->value();
	}

        /**
	 * Checks if KDUMPTOOL_FLAGS contains @p flag.
	 *
	 * @return @c true if KDUMPTOOL_FLAGS contains the flag and @c false
	 *         otherwise
	 * @exception see Configuration::getOption
	 */
	bool kdumptoolContainsFlag(const std::string &flag)
        throw (KError, std::out_of_range);

        /**
	 * Returns KDUMP_SMTP_SERVER.
	 *
	 * @return the STMP server or "" if no SMTP server has been
	 *         specified in the configuration file
	 * @exception see Configuration::getOption
	 */
	std::string getSmtpServer()
        throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(KDUMP_SMTP_SERVER))->value();
	}

	/**
	 * Returns the SMTP username if SMTP AUTH is used. That's the value
	 * of KDUMP_SMTP_USER.
	 *
	 * @return the SMTP user name
	 * @exception see Configuration::getOption
	 */
        std::string getSmtpUser()
        throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(KDUMP_SMTP_USER))->value();
	}

	/**
	 * Returns the SMTP password if SMTP AUTH is used. That's the value
	 * of KDUMP_SMTP_PASSWORD.
	 *
	 * @return the STMP password
	 * @exception see Configuration::getOption
	 */
	std::string getSmtpPassword()
        throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(KDUMP_SMTP_PASSWORD))->value();
	}

	/**
	 * Returns the value of KDUMP_NOTIFICATION_TO.
	 *
	 * @return the notification mail address
	 * @exception see Configuration::getOption
	 */
	std::string getNotificationTo()
        throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(KDUMP_NOTIFICATION_TO))->value();
	}

	/**
	 * Returns the value of KDUMP_NOTIFICATION_CC.
	 *
	 * @return the notification Cc address
	 * @exception see Configuration::getOption
	 */
	std::string getNotificationCc()
        throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(KDUMP_NOTIFICATION_CC))->value();
	}

        /**
	 * Returns the value of KDUMP_HOST_KEY.
	 *
	 * @return the target host key, encoded with base64
	 * @exception see Configuration::getOption
	 */
	std::string getHostKey()
	throw (KError, std::out_of_range)
	{
	    return static_cast<StringConfigOption*>
		(getOption(KDUMP_HOST_KEY))->value();
	}

    protected:
        Configuration()
        throw ();

        virtual ~Configuration()
        throw () {}

	/**
	 * Get a configuration option.  No range check!
	 *
	 * @index  The option index.
	 * @exception KError if the configuration has not been read yet
	 *            std::out_of_range if an invalid index is used
	 */
	ConfigOption *getOption(enum OptionIndex index) const
	throw (KError, std::out_of_range);

    private:
        static Configuration *m_instance;
        bool m_readConfig;

	std::vector<ConfigOption*> m_options;
};

//}}}

#endif /* CONFIGURATION_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

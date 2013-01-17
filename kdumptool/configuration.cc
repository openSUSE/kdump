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
#include <string>

#include "configuration.h"
#include "configparser.h"

using std::string;

//{{{ Configuration ------------------------------------------------------------

#define DEFINE_OPT(name, type, val) \
    { name, Configuration::OptionDesc::type_ ## type,	\
      { .val_ ## type = &Configuration::val } }

const struct Configuration::OptionDesc Configuration::m_optiondesc[] = {
    DEFINE_OPT("KDUMP_KERNELVER", string, m_kernelVersion),
    DEFINE_OPT("KDUMP_CPUS", int, m_CPUs),
    DEFINE_OPT("KDUMP_COMMANDLINE", string, m_commandLine),
    DEFINE_OPT("KDUMP_COMMANDLINE_APPEND", string, m_commandLineAppend),
    DEFINE_OPT("KEXEC_OPTIONS", string, m_kexecOptions),
    DEFINE_OPT("MAKEDUMPFILE_OPTIONS", string, m_makedumpfileOptions),
    DEFINE_OPT("KDUMP_IMMEDIATE_REBOOT", bool, m_immediateReboot),
    DEFINE_OPT("KDUMP_TRANSFER", string, m_customTransfer),
    DEFINE_OPT("KDUMP_SAVEDIR", string, m_savedir),
    DEFINE_OPT("KDUMP_KEEP_OLD_DUMPS", int, m_keepOldDumps),
    DEFINE_OPT("KDUMP_FREE_DISK_SIZE", int, m_freeDiskSize),
    DEFINE_OPT("KDUMP_VERBOSE", int, m_verbose),
    DEFINE_OPT("KDUMP_DUMPLEVEL", int, m_dumpLevel),
    DEFINE_OPT("KDUMP_DUMPFORMAT", string, m_dumpFormat),
    DEFINE_OPT("KDUMP_CONTINUE_ON_ERROR", bool, m_continueOnError),
    DEFINE_OPT("KDUMP_REQUIRED_PROGRAMS", string, m_requiredPrograms),
    DEFINE_OPT("KDUMP_PRESCRIPT", string, m_prescript),
    DEFINE_OPT("KDUMP_POSTSCRIPT", string, m_postscript),
    DEFINE_OPT("KDUMP_COPY_KERNEL", bool, m_copyKernel),
    DEFINE_OPT("KDUMPTOOL_FLAGS", string, m_kdumptoolFlags),
    DEFINE_OPT("KDUMP_NETCONFIG", string, m_netConfig),
    DEFINE_OPT("KDUMP_SMTP_SERVER", string, m_smtpServer),
    DEFINE_OPT("KDUMP_SMTP_USER", string, m_smtpUser),
    DEFINE_OPT("KDUMP_SMTP_PASSWORD", string, m_smtpPassword),
    DEFINE_OPT("KDUMP_NOTIFICATION_TO", string, m_notificationTo),
    DEFINE_OPT("KDUMP_NOTIFICATION_CC", string, m_notificationCc),
    DEFINE_OPT("KDUMP_HOST_KEY", string, m_hostKey),
};

Configuration *Configuration::m_instance = NULL;

// -----------------------------------------------------------------------------
Configuration *Configuration::config()
    throw ()
{
    if (!m_instance)
        m_instance = new Configuration();
    return m_instance;
}

// -----------------------------------------------------------------------------
Configuration::Configuration()
    throw ()
    : m_readConfig(false), m_kernelVersion(""), m_CPUs(1), m_commandLine(""),
      m_commandLineAppend(""), m_kexecOptions(""), m_makedumpfileOptions(""),
      m_immediateReboot(true), m_customTransfer(""), m_savedir("/var/log/dump"),
      m_keepOldDumps(0), m_freeDiskSize(64), m_verbose(0), m_dumpLevel(0),
      m_dumpFormat("compressed"), m_continueOnError(true),
      m_requiredPrograms(""), m_prescript(""), m_postscript(""),
      m_copyKernel(""), m_kdumptoolFlags(""), m_smtpServer(""),
      m_smtpUser(""), m_smtpPassword(""), m_hostKey("")
{}

/* -------------------------------------------------------------------------- */
void Configuration::readFile(const string &filename)
    throw (KError)
{
    unsigned i;
    ConfigParser cp(filename);

    for (i = 0; i < sizeof(m_optiondesc) / sizeof(m_optiondesc[0]); ++i)
	cp.addVariable(m_optiondesc[i].name);

    cp.parse();

    for (i = 0; i < sizeof(m_optiondesc) / sizeof(m_optiondesc[0]); ++i) {
	const struct OptionDesc *opt = &m_optiondesc[i];
	switch(opt->type) {
	case OptionDesc::type_string:
	    *this.*opt->val_string = cp.getValue(opt->name);
	    break;
	case OptionDesc::type_int:
	    *this.*opt->val_int = cp.getIntValue(opt->name);
	    break;
	case OptionDesc::type_bool:
	    *this.*opt->val_bool = cp.getBoolValue(opt->name);
	    break;
	}
    }

    m_readConfig = true;
}

// -----------------------------------------------------------------------------
string Configuration::getKernelVersion() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_kernelVersion;
}

// -----------------------------------------------------------------------------
int Configuration::getCPUs() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_CPUs;
}


// -----------------------------------------------------------------------------
string Configuration::getCommandLine() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_commandLine;
}


// -----------------------------------------------------------------------------
string Configuration::getCommandLineAppend() const
     throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_commandLineAppend;
}


// -----------------------------------------------------------------------------
std::string Configuration::getKexecOptions() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_kexecOptions;
}

// -----------------------------------------------------------------------------
string Configuration::getMakedumpfileOptions() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_makedumpfileOptions;
}

// -----------------------------------------------------------------------------
bool Configuration::getImmediateReboot() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_immediateReboot;
}


// -----------------------------------------------------------------------------
string Configuration::getCustomTransfer() const
     throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_customTransfer;
}


// -----------------------------------------------------------------------------
std::string Configuration::getSavedir() const
     throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_savedir;
}


// -----------------------------------------------------------------------------
int Configuration::getKeepOldDumps() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_keepOldDumps;
}


// -----------------------------------------------------------------------------
int Configuration::getFreeDiskSize() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_freeDiskSize;
}


// -----------------------------------------------------------------------------
int Configuration::getVerbose() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_verbose;
}


// -----------------------------------------------------------------------------
int Configuration::getDumpLevel() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_dumpLevel;
}


// -----------------------------------------------------------------------------
std::string Configuration::getDumpFormat() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_dumpFormat;
}


// -----------------------------------------------------------------------------
bool Configuration::getContinueOnError() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_continueOnError;
}


// -----------------------------------------------------------------------------
std::string Configuration::getRequiredPrograms() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_requiredPrograms;
}


// -----------------------------------------------------------------------------
std::string Configuration::getPrescript() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_prescript;
}


// -----------------------------------------------------------------------------
std::string Configuration::getPostscript() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_postscript;
}


// -----------------------------------------------------------------------------
bool Configuration::getCopyKernel() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_copyKernel;
}

// -----------------------------------------------------------------------------
std::string Configuration::getKdumptoolFlags() const
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_kdumptoolFlags;
}

// -----------------------------------------------------------------------------
bool Configuration::kdumptoolContainsFlag(const std::string &flag)
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    string value = getKdumptoolFlags();
    string::size_type pos = value.find(flag);
    return pos != string::npos;
}

// -----------------------------------------------------------------------------
string Configuration::getSmtpServer()
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_smtpServer;
}

// -----------------------------------------------------------------------------
string Configuration::getSmtpUser()
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_smtpUser;
}

// -----------------------------------------------------------------------------
string Configuration::getSmtpPassword()
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_smtpPassword;
}

// -----------------------------------------------------------------------------
string Configuration::getNotificationTo()
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_notificationTo;
}

// -----------------------------------------------------------------------------
string Configuration::getNotificationCc()
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_notificationCc;
}

// -----------------------------------------------------------------------------
string Configuration::getHostKey()
    throw (KError)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_hostKey;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

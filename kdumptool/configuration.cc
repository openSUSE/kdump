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
    : m_readConfig(false), m_kernelVersion(""), m_commandLine(""),
      m_commandLineAppend(""), m_kexecOptions(""), m_makedumpfileOptions(""),
      m_immediateReboot(true), m_customTransfer(""), m_savedir("/var/log/dump"),
      m_keepOldDumps(0), m_freeDiskSize(64), m_verbose(0), m_dumpLevel(0),
      m_dumpFormat("compressed"), m_continueOnError(true),
      m_requiredPrograms(""), m_prescript(""), m_postscript(""),
      m_copyKernel(""), m_kdumptoolFlags(""), m_smtpServer(""),
      m_smtpUser(""), m_smtpPassword("")
{}

/* -------------------------------------------------------------------------- */
void Configuration::readFile(const string &filename)
    throw (KError)
{
    ConfigParser cp(filename);
    cp.addVariable("KDUMP_KERNELVER");
    cp.addVariable("KDUMP_COMMANDLINE");
    cp.addVariable("KDUMP_COMMANDLINE_APPEND");
    cp.addVariable("KEXEC_OPTIONS");
    cp.addVariable("MAKEDUMPFILE_OPTIONS");
    cp.addVariable("KDUMP_IMMEDIATE_REBOOT");
    cp.addVariable("KDUMP_TRANSFER");
    cp.addVariable("KDUMP_SAVEDIR");
    cp.addVariable("KDUMP_KEEP_OLD_DUMPS");
    cp.addVariable("KDUMP_FREE_DISK_SIZE");
    cp.addVariable("KDUMP_VERBOSE");
    cp.addVariable("KDUMP_DUMPLEVEL");
    cp.addVariable("KDUMP_DUMPFORMAT");
    cp.addVariable("KDUMP_CONTINUE_ON_ERROR");
    cp.addVariable("KDUMP_REQUIRED_PROGRAMS");
    cp.addVariable("KDUMP_PRESCRIPT");
    cp.addVariable("KDUMP_POSTSCRIPT");
    cp.addVariable("KDUMP_COPY_KERNEL");
    cp.addVariable("KDUMPTOOL_FLAGS");
    cp.addVariable("KDUMP_SMTP_SERVER");
    cp.addVariable("KDUMP_SMTP_USER");
    cp.addVariable("KDUMP_SMTP_PASSWORD");
    cp.addVariable("KDUMP_NOTIFICATION_TO");
    cp.addVariable("KDUMP_NOTIFICATION_CC");
    cp.parse();

    m_kernelVersion = cp.getValue("KDUMP_KERNELVER");
    m_commandLine = cp.getValue("KDUMP_COMMANDLINE");
    m_commandLineAppend = cp.getValue("KDUMP_COMMANDLINE_APPEND");
    m_kexecOptions = cp.getValue("KEXEC_OPTIONS");
    m_makedumpfileOptions = cp.getValue("MAKEDUMPFILE_OPTIONS");
    m_immediateReboot = cp.getBoolValue("KDUMP_IMMEDIATE_REBOOT");
    m_customTransfer = cp.getValue("KDUMP_TRANSFER");
    m_savedir = cp.getValue("KDUMP_SAVEDIR");
    m_keepOldDumps = cp.getIntValue("KDUMP_KEEP_OLD_DUMPS");
    m_freeDiskSize = cp.getIntValue("KDUMP_FREE_DISK_SIZE");
    m_verbose = cp.getIntValue("KDUMP_VERBOSE");
    m_dumpLevel = cp.getIntValue("KDUMP_DUMPLEVEL");
    m_dumpFormat = cp.getValue("KDUMP_DUMPFORMAT");
    m_continueOnError = cp.getBoolValue("KDUMP_CONTINUE_ON_ERROR");
    m_requiredPrograms = cp.getValue("KDUMP_REQUIRED_PROGRAMS");
    m_prescript = cp.getValue("KDUMP_PRESCRIPT");
    m_postscript = cp.getValue("KDUMP_POSTSCRIPT");
    m_copyKernel = cp.getBoolValue("KDUMP_COPY_KERNEL");
    m_kdumptoolFlags = cp.getValue("KDUMPTOOL_FLAGS");
    m_smtpServer = cp.getValue("KDUMP_SMTP_SERVER");
    m_smtpUser = cp.getValue("KDUMP_SMTP_USER");
    m_smtpPassword = cp.getValue("KDUMP_SMTP_PASSWORD");
    m_notificationTo = cp.getValue("KDUMP_NOTIFICATION_TO");
    m_notificationCc = cp.getValue("KDUMP_NOTIFICATION_CC");

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

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

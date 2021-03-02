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
#include <iostream>
#include <list>
#include <cstdlib>
#include <vector>
#include <string>
#include <cstdio>

#include "kdumptool.h"
#include "config.h"
#include "subcommand.h"
#include "debug.h"
#include "util.h"
#include "configuration.h"
#include "optionparser.h"
#include "config.h"

using std::list;
using std::cerr;
using std::endl;
using std::exit;
using std::vector;
using std::string;
using std::fopen;
using std::fclose;


#define PROGRAM_NAME                "kdumptool"
#define PROGRAM_VERSION_STRING      PROGRAM_NAME " " PACKAGE_VERSION
#define DEFAULT_CONFIG              "/etc/sysconfig/kdump"

//{{{ KdumpTool ----------------------------------------------------------------

// -----------------------------------------------------------------------------
static void close_file(int error, void *arg)
{
    (void)error;
    fclose((FILE *)arg);
}

// -----------------------------------------------------------------------------
KdumpTool::KdumpTool()
    throw ()
    : m_subcommand(NULL), m_errorcode(false), m_background(false),
      m_configfile(DEFAULT_CONFIG), m_kernel_cmdline()
{}

// -----------------------------------------------------------------------------
KdumpTool::~KdumpTool()
    throw ()
{
    Debug::debug()->trace("KdumpTool::~KdumpTool()");
    delete m_subcommand;
}

// -----------------------------------------------------------------------------
void KdumpTool::addSubcommand(Subcommand *subcommand)
{
    m_subcommandList.push_back(subcommand);
}

// -----------------------------------------------------------------------------
void KdumpTool::parseCommandline(int argc, char *argv[])
    throw (KError)
{
    bool doHelp = false, doVersion = false;
    bool debugEnabled = false;
    string logFilename;
    OptionParser optionParser;
    FlagOption helpOption(
        "help", 'h', &doHelp,
        "Print help output");
    FlagOption versionOption(
        "version", 'v', &doVersion,
        "Print version information and exit");
    FlagOption backgroundOption(
        "background", 'b', &m_background,
        "Run in the background (daemon mode)");
    FlagOption debugOption(
        "debug", 'D', &debugEnabled,
        "Print debugging output");
    StringOption logFileOption(
        "logfile", 'L', &logFilename,
        "Use the specified logfile for debugging output");
    StringOption configFileOption(
        "configfile", 'F', &m_configfile,
        "Use the specified configuration file instead of " DEFAULT_CONFIG);
    StringOption cmdlineOption(
        "cmdline", 'C', &m_kernel_cmdline,
        "Also parse kernel parameters from a given file (e.g. /proc/cmdline)");

    // add global options
    optionParser.addGlobalOption(&helpOption);
    optionParser.addGlobalOption(&versionOption);
    optionParser.addGlobalOption(&backgroundOption);
    optionParser.addGlobalOption(&debugOption);
    optionParser.addGlobalOption(&logFileOption);
    optionParser.addGlobalOption(&configFileOption);
    optionParser.addGlobalOption(&cmdlineOption);

    optionParser.addSubcommands(m_subcommandList);

    optionParser.parse(argc, argv);

    // read the global options
    if (doHelp) {
        optionParser.printHelp(cerr, PROGRAM_VERSION_STRING);
        exit(EXIT_SUCCESS);
    } else if (doVersion) {
        printVersion();
        exit(EXIT_SUCCESS);
    }

    // debug messages
    if (logFileOption.isSet() && debugEnabled) {
        FILE *fp = fopen(logFilename.c_str(), "a");
        if (fp) {
            Debug::debug()->setFileHandle(fp);
            on_exit(close_file, fp);
        }
        Debug::debug()->dbg("STARTUP ----------------------------------");
    } else if (debugEnabled)
        Debug::debug()->setStderrLevel(Debug::DL_TRACE);

    // get subcommand
    m_subcommand = optionParser.getSubcommand();
    if (!m_subcommand)
        throw KError("You must provide a subcommand.");
}

// -----------------------------------------------------------------------------
void KdumpTool::readConfiguration()
    throw (KError)
{
    Debug::debug()->trace("KdumpTool::readConfiguration");

    if (m_subcommand->needsConfigfile()) {
        Configuration *config = Configuration::config();
        config->readFile(m_configfile);
        if (!m_kernel_cmdline.empty())
            config->readCmdLine(m_kernel_cmdline);
    }
}

// -----------------------------------------------------------------------------
void KdumpTool::execute()
    throw (KError)
{
    if (m_background) {
        Debug::debug()->dbg("Daemonize");
        Util::daemonize();
    }

    m_subcommand->execute();
}

// -----------------------------------------------------------------------------
void KdumpTool::printVersion()
{
    cerr << PROGRAM_VERSION_STRING << endl;
    cerr << "Features: ";

    // SFTP
    cerr << "SFTP: " << "enabled";

    cerr << " - ";

    // ESMTP
    cerr << "SMTP: ";
#if HAVE_LIBESMTP
    cerr << "enabled";
#else // HAVE_LIBESMTP
    cerr << "disabled";
#endif

    cerr << endl;
}

// -----------------------------------------------------------------------------
int KdumpTool::getErrorCode() const
    throw ()
{
    if (m_subcommand)
        return m_subcommand->getErrorCode();
    else
        return 0; // use default error because then an exception has been thrown
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

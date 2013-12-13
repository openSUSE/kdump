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
void KdumpTool::parseCommandline(int argc, char *argv[])
    throw (KError)
{
    bool doHelp = false, doVersion = false;
    bool debugEnabled = false;
    string logFilename;

    // add global options
    m_optionParser.addGlobalOption(new FlagOption(
        "help", 'h', &doHelp,
        "Prints help output."));
    m_optionParser.addGlobalOption(new FlagOption(
        "version", 'v', &doVersion,
        "Prints version information and exits."));
    m_optionParser.addGlobalOption(new FlagOption(
        "background", 'b', &m_background,
        "Run in the background (daemon mode)."));
    m_optionParser.addGlobalOption(new FlagOption(
        "debug", 'D', &debugEnabled,
        "Prints debugging output."));
    StringOption *logFileOption = new StringOption(
        "logfile", 'L', &logFilename,
        "Uses the specified logfile for the debugging output.");
    m_optionParser.addGlobalOption(logFileOption);
    m_optionParser.addGlobalOption(new StringOption(
        "configfile", 'F', &m_configfile,
        "Use the specified configuration file instead of "DEFAULT_CONFIG" ."));
    m_optionParser.addGlobalOption(new StringOption(
        "cmdline", 'C', &m_kernel_cmdline,
        "Also parse kernel parameters from a given file (e.g. /proc/cmdline)"));

    // add options of the subcommands
    for (Subcommand::List::iterator it = Subcommand::GlobalList.begin();
            it != Subcommand::GlobalList.end(); ++it)
        m_optionParser.addSubcommand((*it)->getName(), (*it)->getOptions());

    m_optionParser.parse(argc, argv);

    // read the global options
    if (doHelp) {
        m_optionParser.printHelp(cerr, PROGRAM_VERSION_STRING);
        exit(EXIT_SUCCESS);
    } else if (doVersion) {
        printVersion();
        exit(EXIT_SUCCESS);
    }

    // debug messages
    if (logFileOption->isSet() && debugEnabled) {
        FILE *fp = fopen(logFilename.c_str(), "a");
        if (fp) {
            Debug::debug()->setFileHandle(fp);
            on_exit(close_file, fp);
        }
        Debug::debug()->dbg("STARTUP ----------------------------------");
    } else if (debugEnabled)
        Debug::debug()->setStderrLevel(Debug::DL_TRACE);

    // parse arguments
    vector<string> arguments = m_optionParser.getArgs();
    if (arguments.size() < 1)
        throw KError("You must provide a subcommand.");

    Subcommand::List::iterator it;
    for (it = Subcommand::GlobalList.begin();
	    it != Subcommand::GlobalList.end(); ++it) {
	if ((*it)->getName() == arguments[0])
	    break;
    }
    if (it == Subcommand::GlobalList.end())
        throw KError("Subcommand " + arguments[0] + " does not exist.");
    m_subcommand = *it;

    m_subcommand->parseCommandline(&m_optionParser);
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
    cerr << "SFTP: ";
#if HAVE_LIBSSH2
    cerr << "enabled";
#else // HAVE_LIBSSH2
    cerr << "disabled";
#endif

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

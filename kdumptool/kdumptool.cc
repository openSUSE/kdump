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
    // add global options
    m_optionParser.addOption("help", 'h', OT_FLAG,
        "Prints help output.");
    m_optionParser.addOption("version", 'v', OT_FLAG,
        "Prints version information and exits.");
    m_optionParser.addOption("background", 'b', OT_FLAG,
        "Run in the background (daemon mode).");
    m_optionParser.addOption("debug", 'D', OT_FLAG,
        "Prints debugging output.");
    m_optionParser.addOption("logfile", 'L', OT_STRING,
        "Uses the specified logfile for the debugging output.");
    m_optionParser.addOption("configfile", 'F', OT_STRING,
        "Use the specified configuration file instead of " DEFAULT_CONFIG " .");
    m_optionParser.addOption("cmdline", 'C', OT_STRING,
        "Also parse kernel parameters from a given file (e.g. /proc/cmdline)");

    // add options of the subcommands
    SubcommandList subcommands = SubcommandManager::instance()->getSubcommands();
    for (SubcommandList::const_iterator it = subcommands.begin();
            it != subcommands.end(); ++it) {
        m_optionParser.addSubcommand((*it)->getName(), (*it)->getOptions());
    }

    m_optionParser.parse(argc, argv);

    // read the global options
    if (m_optionParser.getValue("help").getFlag()) {
        m_optionParser.printHelp(cerr, PROGRAM_VERSION_STRING);
        exit(EXIT_SUCCESS);
    } else if (m_optionParser.getValue("version").getFlag()) {
        printVersion();
        exit(EXIT_SUCCESS);
    }

    // background
    if (m_optionParser.getValue("background").getFlag())
        m_background = true;

    // debug messages
    bool debugEnabled = m_optionParser.getValue("debug").getFlag();
    if (m_optionParser.getValue("logfile").getType() != OT_INVALID &&
            debugEnabled) {
        string filename = m_optionParser.getValue("logfile").getString();
        FILE *fp = fopen(filename.c_str(), "a");
        if (fp) {
            Debug::debug()->setFileHandle(fp);
            on_exit(close_file, fp);
        }
        Debug::debug()->dbg("STARTUP ----------------------------------");
    } else if (debugEnabled)
        Debug::debug()->setStderrLevel(Debug::DL_TRACE);

    // configuration file
    if (m_optionParser.getValue("configfile").getType() != OT_INVALID)
        m_configfile = m_optionParser.getValue("configfile").getString();

    // kernel command line
    if (m_optionParser.getValue("cmdline").getType() != OT_INVALID)
        m_kernel_cmdline = m_optionParser.getValue("cmdline").getString();

    // parse arguments
    vector<string> arguments = m_optionParser.getArgs();
    if (arguments.size() < 1)
        throw KError("You must provide a subcommand.");

    m_subcommand = SubcommandManager::instance()->getSubcommand(
        arguments[0].c_str());
    if (!m_subcommand)
        throw KError("Subcommand " + arguments[0] + " does not exist.");

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

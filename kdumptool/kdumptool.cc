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

#include "kdumptool.h"
#include "config.h"
#include "subcommand.h"
#include "debug.h"

using std::list;
using std::cerr;
using std::endl;
using std::exit;
using std::vector;
using std::string;


#define PROGRAM_NAME                "kdumptool"
#define PROGRAM_VERSION_STRING      PROGRAM_NAME " " PACKAGE_VERSION

//{{{ KdumpTool ----------------------------------------------------------------

// -----------------------------------------------------------------------------
KdumpTool::KdumpTool()
    throw ()
    : m_errorcode(false)
{}

// -----------------------------------------------------------------------------
void KdumpTool::parseCommandline(int argc, char *argv[])
    throw (KError)
{
    // add global options
    m_optionParser.addOption("help", 'h', OT_FLAG,
        "Prints help output.");
    m_optionParser.addOption("version", 'v', OT_FLAG,
        "Prints version information and exits.");
    m_optionParser.addOption("debug", 'D', OT_FLAG,
        "Prints debugging output.");

    // add options of the subcommands
    SubcommandList subcommands = SubcommandManager::instance()->getSubcommands();
    for (SubcommandList::const_iterator it = subcommands.begin();
            it != subcommands.end(); ++it) {
        m_optionParser.addSubcommand((*it)->getName(), (*it)->getOptions());
    }

    if (!m_optionParser.parse(argc, argv)) {
        throw KError("Error while parsing command line options.");
    }

    // read the global options
    if (m_optionParser.getValue("help").getFlag()) {
        m_optionParser.printHelp(cerr, PROGRAM_VERSION_STRING);
        exit(EXIT_SUCCESS);
    } else if (m_optionParser.getValue("version").getFlag()) {
        cerr << PROGRAM_VERSION_STRING << endl;
        exit(EXIT_SUCCESS);
    } else if (m_optionParser.getValue("debug").getFlag()) {
        Debug::debug()->setLevel(Debug::DL_TRACE);
        Debug::debug()->dbg("Setting debug level to \"TRACE\".");
    }

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
void KdumpTool::execute()
    throw (KError)
{
    m_subcommand->execute();
}

// -----------------------------------------------------------------------------
int KdumpTool::getErrorCode() const
    throw ()
{
    return m_subcommand->getErrorCode();
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

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
#include <cstring>
#include <stdexcept>

#define _GNU_SOURCE 1
#include <getopt.h>
#include <unistd.h>
#include <string.h>

#include "global.h"
#include "optionparser.h"

using std::vector;
using std::string;
using std::cout;
using std::cerr;
using std::pair;
using std::endl;
using std::map;
using std::list;
using std::ostream;
using std::memset;

/* -------------------------------------------------------------------------- */
void OptionParser::addGlobalOption(Option *option)
{
    m_globalOptions.push_back(option);
}

/* -------------------------------------------------------------------------- */
void OptionParser::parse(int argc, char *argv[])
{
    int i = parsePartial(argc, argv, m_globalOptions, false);

    // handle subcommand if present
    m_subcommand = NULL;
    if (i < argc) {
        string subcommand = argv[i++];
        m_args.push_back(subcommand);
        map<string, Subcommand*>::const_iterator it;
        it = m_subcommands.find(subcommand);
        if (it == m_subcommands.end())
            throw KError("Subcommand " + subcommand + " does not exist.");

        m_subcommand = it->second;
        i += parsePartial(argc - i + 1, argv + i - 1,
                          it->second->getOptions()) - 1;
    }

    // save arguments
    while (i < argc)
        m_args.push_back(argv[i++]);
}

/* -------------------------------------------------------------------------- */
int OptionParser::parsePartial(int argc, char *argv[], const OptionList& opts,
    bool rearrange)
{
    OptionList::const_iterator it;
    struct option *cur, opt[opts.size() + 1];
    string   getopt_string;

    if (!rearrange)
        getopt_string = "+";

    // get a struct option array from the map
    cur = opt;
    for (it = opts.begin(); it != opts.end(); ++it)
        getopt_string += (*it)->getoptArgs(cur++);
    memset(cur, 0, sizeof(option));

    // now parse the options
    optind = 1;
    for (;;) {
        int option_index = 0;

        int c = getopt_long(argc, argv, getopt_string.c_str(),
                opt, &option_index);
        if (c == -1)
            break;

	for (it = opts.begin(); it != opts.end(); ++it) {
	    if ((*it)->getLetter() == c) {
		(*it)->setValue(optarg);
		break;
	    }
	}
	if (it == opts.end())
	    throw KError("Invalid command line option");
    }

    return optind;
}

/* -------------------------------------------------------------------------- */
void OptionParser::addSubcommands(const Subcommand::List &subcommands)
{
    Subcommand::List::const_iterator it;
    for (it = subcommands.begin(); it != subcommands.end(); ++it)
        m_subcommands[(*it)->getName()] = *it;
}

// -----------------------------------------------------------------------------
void OptionParser::printHelpForOptionList(ostream &os, const OptionList &opts,
                                          const string &indent) const
{
    for (OptionList::const_iterator it = opts.begin();
            it != opts.end(); ++it) {
        const Option *opt = *it;

        os << indent << "--" << opt->getLongName();
        const char *placeholder = opt->getPlaceholder();
        if (placeholder)
            os << "=" << placeholder;
        os << " | -" << opt->getLetter();
        if (placeholder)
            os << " " << placeholder;
        os << endl;
        os << indent << "     " << opt->getDescription() << endl;
    }
}

/* -------------------------------------------------------------------------- */
void OptionParser::printHelp(ostream &os, const string &name) const
{
    os << name << endl << endl;

    if (m_subcommands.size() > 0) {
        os << "Subcommands" << endl;
        for (map<string, Subcommand*>::const_iterator it = m_subcommands.begin();
                it != m_subcommands.end(); ++it) {
            os << endl;
            os << "   * " << it->first << endl;
        }
        os << endl << endl;
        os << "Global options" << endl << endl;
    }
    printHelpForOptionList(os, m_globalOptions, "   ");
    if (m_subcommands.size() > 0) {
        os << endl;
    }

    for (map<string, Subcommand*>::const_iterator it = m_subcommands.begin();
            it != m_subcommands.end(); ++it) {
        string optionName = it->first;
        const OptionList& options = it->second->getOptions();

        os << "Options for " << optionName << ":" << endl << endl;
        printHelpForOptionList(os, options, "   ");
        os << endl;
    }
}

// vim: set sw=4 ts=4 et fdm=marker: :collapseFolds=1:

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
#include <cstdlib>
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
Option::Option(const string &name, char letter,
               const std::string &description)
    : m_longName(name), m_description(description),
      m_letter(letter), m_isSet(false)
{}

//{{{ FlagOption ---------------------------------------------------------------

/* -------------------------------------------------------------------------- */
FlagOption::FlagOption(const std::string &name, char letter,
                       bool *value,
                       const std::string &description)
    : Option(name, letter, description), m_value(value)
{}

/* -------------------------------------------------------------------------- */
string FlagOption::getoptArgs(struct option *opt)
{
    opt->name = getLongName().c_str();
    opt->has_arg = 0;
    opt->flag = 0;
    opt->val = getLetter();

    char optstring[] = { getLetter(), 0 };
    return string(optstring);
}

/* -------------------------------------------------------------------------- */
void FlagOption::setValue(const char *arg)
{
    m_isSet = true;
    *m_value = true;
}

//}}}
//{{{ StringOption -------------------------------------------------------------

/* -------------------------------------------------------------------------- */
StringOption::StringOption(const std::string &name, char letter,
                           std::string *value,
                           const std::string &description)
    : Option(name, letter, description), m_value(value)
{}

/* -------------------------------------------------------------------------- */
string StringOption::getoptArgs(struct option *opt)
{
    opt->name = getLongName().c_str();
    opt->has_arg = 1;
    opt->flag = 0;
    opt->val = getLetter();

    char optstring[] = { getLetter(), ':', 0 };
    return string(optstring);
}

/* -------------------------------------------------------------------------- */
void StringOption::setValue(const char *arg)
{
    m_isSet = true;
    m_value->assign(arg);
}

//}}}
//{{{ IntOption ----------------------------------------------------------------

/* -------------------------------------------------------------------------- */
IntOption::IntOption(const std::string &name, char letter,
                     int *value,
                     const std::string &description)
    : Option(name, letter, description)
{}

/* -------------------------------------------------------------------------- */
string IntOption::getoptArgs(struct option *opt)
{
    opt->name = getLongName().c_str();
    opt->has_arg = 1;
    opt->flag = 0;
    opt->val = getLetter();

    char optstring[] = { getLetter(), ':', 0 };
    return string(optstring);
}

/* -------------------------------------------------------------------------- */
void IntOption::setValue(const char *arg)
{
    m_isSet = true;
    *m_value = atoi(arg);
}

//}}}

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
    if (i < argc) {
        string subcommand = argv[i++];
        m_args.push_back(subcommand);
        for (StringOptionListVector::const_iterator it = m_subcommandOptions.begin();
                it != m_subcommandOptions.end(); ++it) {
            if (it->first == subcommand)
                i += parsePartial(argc - i + 1, argv + i - 1, *it->second) - 1;
        }
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
vector<string> OptionParser::getArgs()
{
    return m_args;
}

/* -------------------------------------------------------------------------- */
void OptionParser::addSubcommand(const string &name, const OptionList &options)
{
    m_subcommandOptions.push_back(make_pair(name, &options));
}

// -----------------------------------------------------------------------------
template <class InputIterator>
void OptionParser::printHelpForOptionList(ostream &os,
                                          InputIterator begin,
                                          InputIterator end,
                                          const string &indent) const
{
    for (InputIterator it = begin; it != end; ++it) {

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

    if (m_subcommandOptions.size() > 0) {
        os << "Subcommands" << endl;
        for (StringOptionListVector::const_iterator it = m_subcommandOptions.begin();
                it != m_subcommandOptions.end(); ++it) {
            os << endl;
            os << "   * " << it->first << endl;
        }
        os << endl << endl;
        os << "Global options" << endl << endl;
    }
    printHelpForOptionList(os, m_globalOptions.begin(), m_globalOptions.end(),
        "   ");
    if (m_subcommandOptions.size() > 0) {
        os << endl;
    }

    for (StringOptionListVector::const_iterator it = m_subcommandOptions.begin();
            it != m_subcommandOptions.end(); ++it) {
        string optionName = it->first;
        const OptionList *options = it->second;

        os << "Options for " << optionName << ":" << endl << endl;
        printHelpForOptionList(os, options->begin(), options->end(), "   ");
        os << endl;
    }
}

// vim: set sw=4 ts=4 et fdm=marker: :collapseFolds=1:

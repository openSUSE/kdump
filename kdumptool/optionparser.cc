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
OptionValue::OptionValue()
    : m_type(OT_INVALID), m_integer(0), m_string(""), m_flag(false)
{}

/* -------------------------------------------------------------------------- */
void OptionValue::setType(OptionType type)
{
    m_type = type;
}

/* -------------------------------------------------------------------------- */
void OptionValue::setString(const string &string)
{
    m_string = string;
}

/* -------------------------------------------------------------------------- */
void OptionValue::setFlag(bool flag)
{
    m_flag = flag;
}

/* -------------------------------------------------------------------------- */
void OptionValue::setInteger(int value)
{
    m_integer = value;
}

/* -------------------------------------------------------------------------- */
Option::Option(const string &name, char letter, OptionType type,
               const std::string &description)
    : m_longName(name), m_description(description),
      m_letter(letter), m_type(type)
{}

/* -------------------------------------------------------------------------- */
void Option::setValue(OptionValue value)
{
    m_value = value;
}

//{{{ FlagOption ---------------------------------------------------------------

/* -------------------------------------------------------------------------- */
FlagOption::FlagOption(const std::string &name, char letter,
                       const std::string &description)
    : Option(name, letter, OT_FLAG, description)
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

//}}}
//{{{ StringOption -------------------------------------------------------------

/* -------------------------------------------------------------------------- */
StringOption::StringOption(const std::string &name, char letter,
                           const std::string &description)
    : Option(name, letter, OT_STRING, description)
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

//}}}
//{{{ IntOption ----------------------------------------------------------------

/* -------------------------------------------------------------------------- */
IntOption::IntOption(const std::string &name, char letter,
                     const std::string &description)
    : Option(name, letter, OT_INTEGER, description)
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

//}}}

/* -------------------------------------------------------------------------- */
void OptionParser::addOption(Option *option)
{
    m_options.push_back(option);
    m_globalOptions.push_back(option);
}

/* -------------------------------------------------------------------------- */
void OptionParser::parse(int argc, char *argv[])
{
    struct option *cur, *opt;
    string   getopt_string;

    opt = new option[m_options.size() + 1];
    cur = opt;

    // get a struct option array from the map
    for (vector<Option*>::iterator it = m_options.begin();
            it != m_options.end(); ++it)
	getopt_string += (*it)->getoptArgs(cur++);
    memset(cur, 0, sizeof(option));

    // now parse the options
    int c;

    for (;;) {
        int option_index = 0;

        c = getopt_long(argc, argv, getopt_string.c_str(),
                opt, &option_index);
        if (c == -1)
            break;

        Option &current_option = findOption(c);
        OptionValue v;
        v.setType(current_option.getType());
        switch (current_option.getType()) {
            case OT_FLAG:
                v.setFlag(true);
                current_option.setValue(v);
                break;

            case OT_INTEGER:
                v.setInteger(atoi(optarg));
                break;

            case OT_STRING:
                v.setString(string(optarg));
                break;

            default:
                break;

        }
        current_option.setValue(v);

    }

    // save arguments
    if (optind < argc)
        while (optind < argc)
            m_args.push_back(argv[optind++]);

    // free stuff
    delete[] opt;
}

/* -------------------------------------------------------------------------- */
OptionValue OptionParser::getValue(const string &name)
{
    for (vector<Option*>::iterator it = m_options.begin();
            it != m_options.end(); ++it) {
        Option *opt = *it;

        if (opt->getLongName() == name)
            return opt->getValue();
    }

    return OptionValue();
}

/* -------------------------------------------------------------------------- */
Option &OptionParser::findOption(char letter)
{
    // get a struct option array from the map
    for (vector<Option*>::iterator it = m_options.begin();
            it != m_options.end(); ++it) {

        Option *opt = *it;
        if (opt->getLetter() == letter)
            return *opt;
    }

    throw std::out_of_range(string("Invalid option: ") + letter);
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
    m_options.insert(m_options.end(), options.begin(), options.end());
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

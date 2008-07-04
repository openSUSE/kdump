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
OptionType OptionValue::getType() const
{
    return m_type;
}

/* -------------------------------------------------------------------------- */
void OptionValue::setString(const string &string)
{
    m_string = string;
}

/* -------------------------------------------------------------------------- */
string OptionValue::getString() const
{
    return m_string;
}

/* -------------------------------------------------------------------------- */
void OptionValue::setFlag(bool flag)
{
    m_flag = flag;
}

/* -------------------------------------------------------------------------- */
bool OptionValue::getFlag() const
{
    return m_flag;
}

/* -------------------------------------------------------------------------- */
void OptionValue::setInteger(int value)
{
    m_integer = value;
}

/* -------------------------------------------------------------------------- */
int OptionValue::getInteger() const
{
    return m_integer;
}

/* -------------------------------------------------------------------------- */
Option::Option()
    : m_type(OT_FLAG)
{}

/* -------------------------------------------------------------------------- */
Option::Option(const string &name, char letter, OptionType type,
               const std::string &description)
    : m_longName(name), m_description(description),
      m_letter(letter), m_type(type)
{}

/* -------------------------------------------------------------------------- */
void Option::setLongName(const string &name)
{
    m_longName = name;
}

/* -------------------------------------------------------------------------- */
string Option::getLongName() const
{
    return m_longName;
}

/* -------------------------------------------------------------------------- */
void Option::setLetter(char letter)
{
    m_letter = letter;
}

/* -------------------------------------------------------------------------- */
char Option::getLetter() const
{
    return m_letter;
}

/* -------------------------------------------------------------------------- */
void Option::setType(OptionType type)
{
    m_type = type;
}

/* -------------------------------------------------------------------------- */
OptionType Option::getType() const
{
    return m_type;
}

/* -------------------------------------------------------------------------- */
void Option::setDescription(const std::string &description)
{
    m_description = description;
}

/* -------------------------------------------------------------------------- */
string Option::getDescription() const
{
    return m_description;
}

/* -------------------------------------------------------------------------- */
void Option::setValue(OptionValue value)
{
    m_value = value;
}

/* -------------------------------------------------------------------------- */
OptionValue Option::getValue() const
{
    return m_value;
}

/* -------------------------------------------------------------------------- */
bool Option::isValid() const
{
    return m_longName.size() > 0 && m_letter != 0;
}

/* -------------------------------------------------------------------------- */
string Option::getPlaceholder() const
{
    switch (getType()) {
        case OT_STRING:
            return "<STRING>";

        case OT_INTEGER:
            return "<NUMBER>";

        default:
            return "";
    }
}

/* -------------------------------------------------------------------------- */
void OptionParser::addOption(Option option)
{
    m_options.push_back(option);
    m_globalOptions.push_back(option);
}

/* -------------------------------------------------------------------------- */
void OptionParser::addOption(const string &name, char letter,
        OptionType type, const std::string &description)
{
    Option op(name, letter, type, description);
    addOption(op);
}

/* -------------------------------------------------------------------------- */
bool OptionParser::parse(int argc, char *argv[])
{
    struct option *cur, *opt;
    string   getopt_string;

    opt = new option[m_options.size() + 1];
    if (!opt) {
        cerr << "OptionParser::parse(): malloc failed" << endl;
        return false;
    }
    cur = opt;

    // get a struct option array from the map
    for (vector<Option>::iterator it = m_options.begin();
            it != m_options.end(); ++it) {
        Option opt = *it;
        cur->name = strdup(opt.getLongName().c_str());
        cur->has_arg = opt.getType() != OT_FLAG;
        cur->flag = 0;
        cur->val = opt.getLetter();

        getopt_string += opt.getLetter();
        if (opt.getType() != OT_FLAG)
            getopt_string += ":";

        cur++;
    }
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
        if (!current_option.isValid()) {
            cerr << "Invalid option: " << (char)c << endl;
            break;
        }

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
    cur = opt;
    for (unsigned int i = 0; i < m_options.size(); i++) {
        free((void *)cur->name);
        cur++;
    }
    delete[] opt;

    return true;
}

/* -------------------------------------------------------------------------- */
OptionValue OptionParser::getValue(const string &name)
{
    for (vector<Option>::iterator it = m_options.begin();
            it != m_options.end(); ++it) {
        Option &op = *it;

        if (op.getLongName() == name)
            return op.getValue();
    }

    return OptionValue();
}

/* -------------------------------------------------------------------------- */
Option &OptionParser::findOption(char letter)
{
    static Option invalid;

    // get a struct option array from the map
    for (vector<Option>::iterator it = m_options.begin();
            it != m_options.end(); ++it) {

        Option &opt = *it;
        if (opt.getLetter() == letter)
            return opt;
    }

    return invalid;
}

/* -------------------------------------------------------------------------- */
vector<string> OptionParser::getArgs()
{
    return m_args;
}

/* -------------------------------------------------------------------------- */
void OptionParser::addSubcommand(const string &name, const OptionList &options)
{
    m_subcommandOptions.push_back(make_pair(name, options));
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

        const Option &opt = *it;

        os << indent << "--" << opt.getLongName();
        string placeholder = opt.getPlaceholder();
        if (placeholder.length() > 0)
            os << "=" << opt.getPlaceholder();
        os << " | -" << opt.getLetter();
        if (placeholder.length() > 0)
            os << " " << opt.getPlaceholder();
        os << endl;
        os << indent << "     " << opt.getDescription() << endl;
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
        OptionList options = it->second;

        os << "Options for " << optionName << ":" << endl << endl;
        printHelpForOptionList(os, options.begin(), options.end(), "   ");
        os << endl;
    }
}

// vim: set sw=4 ts=4 et fdm=marker: :collapseFolds=1:

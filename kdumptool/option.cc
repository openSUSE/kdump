/*
 * (c) 2013, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
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
#include <cstdlib>

#define _GNU_SOURCE 1
#include <getopt.h>

#include "option.h"

using std::string;

//{{{ Option -------------------------------------------------------------------

/* -------------------------------------------------------------------------- */
Option::Option(const string &name, char letter,
               const string &description)
    : m_longName(name), m_description(description),
      m_letter(letter), m_isSet(false)
{}
//}}}
//{{{ FlagOption ---------------------------------------------------------------

/* -------------------------------------------------------------------------- */
FlagOption::FlagOption(const string &name, char letter,
                       bool *value,
                       const string &description)
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
StringOption::StringOption(const string &name, char letter,
                           string *value,
                           const string &description)
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
IntOption::IntOption(const string &name, char letter,
                     int *value,
                     const string &description)
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

// vim: set sw=4 ts=4 et fdm=marker: :collapseFolds=1:

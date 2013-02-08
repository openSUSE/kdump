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
#include <fstream>
#include <sstream>

#include "configparser.h"
#include "debug.h"
#include "stringutil.h"
#include "process.h"
#include "quotedstring.h"

using std::string;
using std::ifstream;
using std::stringstream;

//{{{ ConfigParser -------------------------------------------------------------

// -----------------------------------------------------------------------------
void ConfigParser::addVariable(const string &name, const string &defvalue)
    throw ()
{
    Debug::debug()->trace("ConfigParser: Adding %s to variable list"
	" (default: '%s')", name.c_str(), defvalue.c_str());

    // add the default value to the map
    m_variables[name] = defvalue;
}

// -----------------------------------------------------------------------------
string ConfigParser::getValue(const std::string &name) const
    throw (KError)
{
    StringStringMap::const_iterator loc = m_variables.find(name);

    if (loc == m_variables.end())
        throw KError("Variable " + name + " does not exist.");

    return loc->second;
}

//}}}

//{{{ ShellConfigParser --------------------------------------------------------

// -----------------------------------------------------------------------------
ShellConfigParser::ShellConfigParser(const string &filename)
    throw ()
    : ConfigParser(filename)
{}

// -----------------------------------------------------------------------------
void ShellConfigParser::parse()
    throw (KError)
{
    // check if the configuration file does exist
    ifstream fin(m_configFile.c_str());
    if (!fin)
        throw KError("The file " + m_configFile + " does not exist.");
    fin.close();

    // build the shell snippet
    string shell ("#!/bin/sh\n");

    // set default values
    for (StringStringMap::const_iterator it = m_variables.begin();
            it != m_variables.end(); ++it) {
        const string name = it->first;
	ShellQuotedString value(it->second);

        shell += name + "=" + value.quoted() + "\n";
    }

    shell += "source " + m_configFile + "\n";

    for (StringStringMap::const_iterator it = m_variables.begin();
            it != m_variables.end(); ++it) {
        const string name = it->first;

        shell += "echo '" + name + "='$" + name + "\n";
    }

    ByteVector shellInputBytes = Stringutil::str2bytes(shell);
    ByteVector shellOutputBytes;

    Process p;
    p.setStdinBuffer(&shellInputBytes);
    p.setStdoutBuffer(&shellOutputBytes);
    p.execute("/bin/sh", StringVector());

    string shelloutput = Stringutil::bytes2str(shellOutputBytes);
    stringstream ss;

    ss << shelloutput;

    string s;
    int no = 1;
    while (getline(ss, s)) {
        Debug::debug()->trace("ShellConfigParser: Parsing line %s", s.c_str());

        string::size_type loc = s.find('=');
        if (loc == string::npos)
            throw KError("Parsing line number " +
                Stringutil::number2string(no) + " failed.");

        string name = s.substr(0, loc);
        string value = s.substr(loc+1);

        Debug::debug()->trace("ShellConfigParser: Setting %s to %s",
            name.c_str(), value.c_str());

        m_variables[name] = value;
    }
}

//}}}

//{{{ KernelConfigParser -------------------------------------------------------

// -----------------------------------------------------------------------------
static bool is_kernel_space(const char c)
{
    // follow the definition from lib/ctype.c in the Linux kernel tree
    return (c >= 9 && c <= 13) || c == 32 || c == (char)160;
}

// -----------------------------------------------------------------------------
KernelConfigParser::KernelConfigParser(const string &filename)
    throw ()
    : ConfigParser(filename)
{}

// -----------------------------------------------------------------------------
void KernelConfigParser::parse()
    throw (KError)
{
    // check if the configuration file does exist
    ifstream fin(m_configFile.c_str());
    if (!fin)
        throw KError("The file " + m_configFile + " does not exist.");

    // slurp the file into a string stream
    stringstream ss;
    ss << fin.rdbuf();
    fin.close();

    string name, value, *outp = &name;
    bool inquote = false;
    char c;
    while (ss.get(c)) {
        if (c == '\"') {
            inquote = !inquote;
            continue;
        }
        if (c == '=' && outp == &name) {
            outp = &value;
            continue;
        }
        if (is_kernel_space(c) && !inquote) {
            if (name.empty())
                continue;

            Debug::debug()->trace("KernelConfigParser: Setting %s to %s",
                name.c_str(), value.c_str());

            m_variables[name] = value;

            name.clear();
            value.clear();
            outp = &name;
        } else {
            outp->push_back(c);
        }
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

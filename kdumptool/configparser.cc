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

using std::string;
using std::ifstream;
using std::stringstream;

//{{{ ConfigParser -------------------------------------------------------------

// -----------------------------------------------------------------------------
ConfigParser::ConfigParser(const string &filename)
    throw ()
    : m_configFile(filename)
{}

// -----------------------------------------------------------------------------
void ConfigParser::addVariable(const string &name)
    throw ()
{
    Debug::debug()->trace("ConfigParser: Adding %s to variable list.",
        name.c_str());

    // add an empty string to the map
    m_variables[name] = string();
}

// -----------------------------------------------------------------------------
void ConfigParser::parse()
    throw (KError)
{
    // build the shell snippet
    string shell;

    // check if the configuration file does exist
    ifstream fin(m_configFile.c_str());
    if (!fin)
        throw KError("The file " + m_configFile + " does not exist.");
    fin.close();

    shell += "#!/bin/sh\n";
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
        Debug::debug()->trace("ConfigParser: Parsing line %s", s.c_str());

        string::size_type loc = s.find('=');
        if (loc == string::npos)
            throw KError("Parsing line number " +
                Stringutil::number2string(no) + " failed.");

        string name = s.substr(0, loc);
        string value = s.substr(loc+1);

        Debug::debug()->trace("ConfigParser: Setting %s to %s",
            name.c_str(), value.c_str());

        m_variables[name] = value;
    }
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

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

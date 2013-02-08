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
#include <sstream>

#include "configuration.h"
#include "configparser.h"
#include "stringutil.h"

using std::string;

//{{{ ConfigOption -------------------------------------------------------------
void ConfigOption::registerVar(ConfigParser &cp) const
{
    cp.addVariable(m_name, valueAsString());
}

//{{{ StringConfigOption -------------------------------------------------------
string StringConfigOption::valueAsString() const
    throw ()
{
    return m_value;
}

// -----------------------------------------------------------------------------
void StringConfigOption::update(ConfigParser &cp)
    throw (KError)
{
    m_value = cp.getValue(m_name);
}

//}}}

//{{{ IntConfigOption ----------------------------------------------------------
string IntConfigOption::valueAsString() const
    throw ()
{
    return Stringutil::number2string(m_value);
}

// -----------------------------------------------------------------------------
void IntConfigOption::update(ConfigParser &cp)
    throw (KError)
{
    std::stringstream ss;
    ss << cp.getValue(m_name);
    ss >> m_value;
}

//}}}

//{{{ BoolConfigOption -------------------------------------------------------
string BoolConfigOption::valueAsString() const
    throw ()
{
    return m_value ? "yes" : "no";
}

// -----------------------------------------------------------------------------
void BoolConfigOption::update(ConfigParser &cp)
    throw (KError)
{
    string val = cp.getValue(m_name);
    for (string::iterator it = val.begin(); it != val.end(); ++it)
	*it = tolower(*it);
    m_value = !(val == "0" || val == "false" || val == "no");
}

//}}}

//{{{ Configuration ------------------------------------------------------------

Configuration *Configuration::m_instance = NULL;

// -----------------------------------------------------------------------------
Configuration *Configuration::config()
    throw ()
{
    if (!m_instance)
        m_instance = new Configuration();
    return m_instance;
}

// -----------------------------------------------------------------------------
Configuration::Configuration()
    throw ()
    : m_readConfig(false)
{
#define MKINITRD (1<<ConfigOption::USE_MKINITRD)
#define KEXEC    (1<<ConfigOption::USE_KEXEC)
#define DUMP     (1<<ConfigOption::USE_DUMP)
#define DEFINE_OPT(name, type, defval, usage)				\
    m_options.push_back(new type ## ConfigOption(#name, usage, defval));
#include "define_opt.h"
#undef MKINITRD
#undef KEXEC
#undef DUMP
#undef DEFINE_OPT
}

/* -------------------------------------------------------------------------- */
void Configuration::readFile(const string &filename)
    throw (KError)
{
    ConfigParser cp(filename);

    std::vector<ConfigOption*>::iterator it;
    for (it = m_options.begin(); it != m_options.end(); ++it)
	(*it)->registerVar(cp);

    cp.parse();

    for (it = m_options.begin(); it != m_options.end(); ++it)
	(*it)->update(cp);

    m_readConfig = true;
}

// -----------------------------------------------------------------------------
ConfigOption *Configuration::getOptionPtr(enum OptionIndex index) const
    throw (KError, std::out_of_range)
{
    if (!m_readConfig)
        throw KError("Configuration has not been read.");

    return m_options.at(index);
}

// -----------------------------------------------------------------------------
bool Configuration::kdumptoolContainsFlag(const std::string &flag)
    throw (KError, std::out_of_range, std::bad_cast)
{
    const string &value = getStringValue(KDUMPTOOL_FLAGS);
    string::size_type pos = value.find(flag);
    return pos != string::npos;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

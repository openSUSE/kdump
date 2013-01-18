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

#include "configuration.h"
#include "configparser.h"

using std::string;

//{{{ StringConfigOption -------------------------------------------------------
void StringConfigOption::update(ConfigParser &cp)
    throw (KError)
{
    m_value = cp.getValue(m_name);
}

//}}}

//{{{ IntConfigOption ----------------------------------------------------------
void IntConfigOption::update(ConfigParser &cp)
    throw (KError)
{
    m_value = cp.getIntValue(m_name);
}

//}}}

//{{{ BoolConfigOption -------------------------------------------------------
void BoolConfigOption::update(ConfigParser &cp)
    throw (KError)
{
    m_value = cp.getBoolValue(m_name);
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
#define DEFINE_OPT(name, type, defval) \
    m_options.push_back(new type ## ConfigOption(#name, defval));
#include "define_opt.h"
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
    string value = getKdumptoolFlags();
    string::size_type pos = value.find(flag);
    return pos != string::npos;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

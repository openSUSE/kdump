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
#include <strings.h>

#include "configuration.h"
#include "configparser.h"
#include "stringutil.h"
#include "rootdirurl.h"

using std::string;

//{{{ StringConfigOption -------------------------------------------------------
string StringConfigOption::valueAsString() const
{
    return m_value;
}

// -----------------------------------------------------------------------------
void StringConfigOption::update(const string &value)
{
    m_value = value;
}

//}}}

//{{{ IntConfigOption ----------------------------------------------------------
string IntConfigOption::valueAsString() const
{
    return StringUtil::number2string(m_value);
}

// -----------------------------------------------------------------------------
void IntConfigOption::update(const string &value)
{
    std::stringstream ss;
    ss << value;
    ss >> m_value;
}

//}}}

//{{{ BoolConfigOption -------------------------------------------------------
string BoolConfigOption::valueAsString() const
{
    return m_value ? "yes" : "no";
}

// -----------------------------------------------------------------------------
void BoolConfigOption::update(const string &value)
{
    string val = value;
    for (string::iterator it = val.begin(); it != val.end(); ++it)
	*it = tolower(*it);
    m_value = !(val == "0" || val == "false" || val == "no");
}

//}}}

//{{{ Configuration ------------------------------------------------------------

Configuration *Configuration::m_instance = NULL;

// -----------------------------------------------------------------------------
Configuration *Configuration::config()
{
    if (!m_instance)
        m_instance = new Configuration();
    return m_instance;
}

// -----------------------------------------------------------------------------
Configuration::Configuration()
    :
#define MKINITRD (1<<ConfigOption::USE_MKINITRD)
#define KEXEC    (1<<ConfigOption::USE_KEXEC)
#define DUMP     (1<<ConfigOption::USE_DUMP)
#define DEFINE_OPT(name, type, defval, usage)				\
    name (#name, usage, defval),
#include "define_opt.h"
#undef MKINITRD
#undef KEXEC
#undef DUMP
#undef DEFINE_OPT
    m_readConfig(false)
{
    m_options.reserve(optionCount);
#define DEFINE_OPT(name, type, defval, usage)				\
    m_options.push_back(&name);
#include "define_opt.h"
#undef DEFINE_OPT
}

/* -------------------------------------------------------------------------- */
void Configuration::readFile(const string &filename)
{
    ShellConfigParser cp(filename);

    std::vector<ConfigOption*>::iterator it;
    for (it = m_options.begin(); it != m_options.end(); ++it) {
        ConfigOption *opt = *it;
        cp.addVariable(opt->name(), opt->valueAsString());
    }

    cp.parse();

    for (it = m_options.begin(); it != m_options.end(); ++it) {
        ConfigOption *opt = *it;
        opt->update(cp.getValue(opt->name()));
    }

    m_readConfig = true;
}

// -----------------------------------------------------------------------------
bool Configuration::kdumptoolContainsFlag(const std::string &flag)
{
    string::size_type pos = KDUMPTOOL_FLAGS.value().find(flag);
    return pos != string::npos;
}

// -----------------------------------------------------------------------------
bool Configuration::needsNetwork()
{
    const string &netconfig = KDUMP_NETCONFIG.value();

    // easy cases first
    if (netconfig.empty())
	return false;

    if (netconfig != "auto")
	return true;

    std::istringstream iss(KDUMP_SAVEDIR.value());
    string elem;
    while (iss >> elem) {
        URLParser url(elem);
        if (url.getProtocol() != URLParser::PROT_FILE)
	    return true;
    }

    return !KDUMP_SMTP_SERVER.value().empty() &&
	!KDUMP_NOTIFICATION_TO.value().empty();
}

// -----------------------------------------------------------------------------
bool Configuration::needsMakedumpfile()
{
    if (KDUMP_DUMPLEVEL.value() != 0)
	return true;

    return strcasecmp(KDUMP_DUMPFORMAT.value().c_str(), "none") != 0 &&
	strcasecmp(KDUMP_DUMPFORMAT.value().c_str(), "elf") != 0;
}


//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

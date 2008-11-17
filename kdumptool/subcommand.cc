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
#include <map>
#include <list>
#include <string>

#include "subcommand.h"
#include "identifykernel.h"
#include "ledblink.h"
#include "debug.h"
#include "savedump.h"
#include "read_vmcoreinfo.h"
#include "deletedumps.h"
#include "print_target.h"
#include "read_ikconfig.h"

using std::map;
using std::list;
using std::string;


//{{{ Subcommand ---------------------------------------------------------------

// -----------------------------------------------------------------------------
Subcommand::Subcommand()
    throw ()
    : m_errorcode(0)
{}

// -----------------------------------------------------------------------------
void Subcommand::setErrorCode(int errorcode)
    throw ()
{
    Debug::debug()->dbg("set error code to %d", errorcode);
    m_errorcode = errorcode;
}

// -----------------------------------------------------------------------------
int Subcommand::getErrorCode() const
    throw ()
{
    Debug::debug()->trace("%s: Returning error code of %d", __FUNCTION__,
        m_errorcode);
    return m_errorcode;
}

// -----------------------------------------------------------------------------
OptionList Subcommand::getOptions() const
    throw ()
{
    return OptionList();
}

// -----------------------------------------------------------------------------
bool Subcommand::needsConfigfile() const
    throw ()
{
    return true;
}

//}}}
//{{{ SubcommandManager --------------------------------------------------------
SubcommandManager *SubcommandManager::m_instance = NULL;

// -----------------------------------------------------------------------------
Subcommand *SubcommandManager::getSubcommand(const char *name) const
    throw ()
{
    map<string, Subcommand *>::const_iterator it;
    it = m_subcommandMap.find(string(name));
    if (it == m_subcommandMap.end())
        return NULL;
    else
        return it->second;
}

// -----------------------------------------------------------------------------
void SubcommandManager::addSubcommand(Subcommand *command)
    throw ()
{
    m_subcommandMap[command->getName()] = command;
}

// -----------------------------------------------------------------------------
SubcommandManager *SubcommandManager::instance()
    throw ()
{
    if (!m_instance)
        m_instance = new SubcommandManager();

    return m_instance;
}

// -----------------------------------------------------------------------------
SubcommandManager::SubcommandManager()
    throw ()
{
    addSubcommand(new IdentifyKernel());
    addSubcommand(new SaveDump());
    addSubcommand(new LedBlink());
    addSubcommand(new ReadVmcoreinfo());
    addSubcommand(new DeleteDumps());
    addSubcommand(new PrintTarget());
    addSubcommand(new ReadIKConfig());
}

// -----------------------------------------------------------------------------
list<Subcommand *> SubcommandManager::getSubcommands() const
    throw ()
{
    list<Subcommand *> ret;
    map<string, Subcommand *>::const_iterator it;

    for (it =  m_subcommandMap.begin(); it != m_subcommandMap.end(); ++it)
        ret.push_back(it->second);

    return ret;
}

//}}}

// vim: set sw=4 ts=4 et fdm=marker: :collapseFolds=1:

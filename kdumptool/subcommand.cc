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

#include "subcommand.h"
#include "debug.h"

using std::string;

Subcommand::List Subcommand::GlobalList
    __attribute__((init_priority(1000)));

//{{{ Subcommand ---------------------------------------------------------------

// -----------------------------------------------------------------------------
Subcommand::Subcommand(List &list)
    throw ()
    : m_options(), m_errorcode(0)
{
    list.push_back(this);
}

// -----------------------------------------------------------------------------
Subcommand::~Subcommand()
{
    for (OptionList::iterator it = m_options.begin();
            it != m_options.end(); ++it)
	delete *it;
}

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
bool Subcommand::needsConfigfile() const
    throw ()
{
    return true;
}

// -----------------------------------------------------------------------------
void Subcommand::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace("Subcommand::parseCommandline(%p)", optionparser);
}

//}}}

// vim: set sw=4 ts=4 et fdm=marker: :collapseFolds=1:

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
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <netdb.h>

#include "global.h"

#include <gelf.h>

using std::strerror;
using std::string;

//{{{ KSystemErrorCode ---------------------------------------------------------

// -----------------------------------------------------------------------------
string KSystemErrorCode::message(void) const
    throw ()
{
    return string(strerror(getCode()));
}

//}}}
//{{{ KNetErrorCode ------------------------------------------------------------

// -----------------------------------------------------------------------------
string KNetErrorCode::message(void) const
    throw ()
{
    return string(hstrerror(getCode()));
}

//}}}
//{{{ KGaiErrorCode ------------------------------------------------------------

// -----------------------------------------------------------------------------
string KGaiErrorCode::message(void) const
    throw ()
{
    return string(gai_strerror(getCode()));
}

//}}}
//{{{ KELFErrorCode ------------------------------------------------------------

// -----------------------------------------------------------------------------
string KELFErrorCode::message(void) const
    throw ()
{
    return string(elf_errmsg(getCode()));
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

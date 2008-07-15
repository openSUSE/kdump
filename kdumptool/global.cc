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
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>

#include "global.h"

#define MAXERROR 4096

using std::strerror;
using std::string;

// -----------------------------------------------------------------------------
const char *KSystemError::what() const
    throw ()
{
    static char buffer[MAXERROR];

    string errorstring = m_errorstring + " (" + strerror(m_errorcode) + ")";

    strncpy(buffer, errorstring.c_str(), MAXERROR);
    buffer[MAXERROR-1] = 0;

    return buffer;
}

/* -------------------------------------------------------------------------- */
const char *KNetError::what() const
    throw ()
{
    static char buffer[MAXERROR];

    string errorstring = m_errorstring + " (" + hstrerror(m_errorcode) + ")";

    strncpy(buffer, errorstring.c_str(), MAXERROR);
    buffer[MAXERROR-1] = 0;

    return buffer;
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

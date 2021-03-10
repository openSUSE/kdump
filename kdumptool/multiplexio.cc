/*
 * (c) 2021, Petr Tesarik <ptesarik@suse.de>, SUSE Linux Software Solutions GmbH
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

#include <cerrno>
#include <poll.h>

#include "global.h"
#include "multiplexio.h"

//{{{ MultiplexIO --------------------------------------------------------------

// -----------------------------------------------------------------------------
int MultiplexIO::add(int fd, short events)
{
    if (fd >= 0)
	++m_active;

    struct pollfd poll;
    poll.fd = fd;
    poll.events = events;
    m_fds.push_back(poll);

    return m_fds.size() - 1;
}

// -----------------------------------------------------------------------------
struct pollfd &MultiplexIO::operator[](int idx)
{
    return m_fds.at(idx);
}

// -----------------------------------------------------------------------------
void MultiplexIO::deactivate(int idx)
{
    if (m_fds[idx].fd >= 0)
	--m_active;
    m_fds[idx].fd = -1;
}

// -----------------------------------------------------------------------------
int MultiplexIO::monitor(int timeout)
{
    int ret;

    do {
	ret = poll(m_fds.data(), m_fds.size(), timeout);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0)
	throw KSystemError("poll() failed", errno);

    return ret;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

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
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include "global.h"
#include "socket.h"
#include "debug.h"

using std::memcpy;

//{{{ Socket -------------------------------------------------------------------

Socket::Socket(const char *address, int port, Socket::Type type)
    throw ()
    : m_currentFd(-1), m_hostname(address), m_port(port), m_connectionType(type)
{
    Debug::debug()->trace("Socket(%s, %d, %d)", address, port, type);
}

// -----------------------------------------------------------------------------
Socket::~Socket()
    throw ()
{
    Debug::debug()->trace("Socket::~Socket()");
    close();
}

// -----------------------------------------------------------------------------
int Socket::connect()
    throw (KError)
{
    struct in_addr inaddr;
    struct sockaddr_in addr;
    struct hostent *machine;

    Debug::debug()->trace("Socket::connect()");

    int ret = inet_aton(m_hostname.c_str(), &inaddr);
    if (ret) {
        Debug::debug()->trace("Socket::connect(): Using numerical IP address");
	memcpy(&addr.sin_addr, &inaddr, sizeof(addr.sin_addr));
    } else {
        Debug::debug()->trace("Socket::connect(): Using gethostbyname()");
        machine = gethostbyname(m_hostname.c_str());
        if (!machine)
            throw KNetError("gethostbyname() failed for "+ m_hostname +".",
                h_errno);
	memcpy(&addr.sin_addr, machine->h_addr_list[0], sizeof(addr.sin_addr));
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);

    // UDP or TCP ?
    int type = m_connectionType == Socket::ST_TCP
        ? SOCK_STREAM
        : SOCK_DGRAM;

    Debug::debug()->trace("Socket::connect(): socket(%d, %d, 0)",
        PF_INET, type);

    m_currentFd = socket(PF_INET, type, 0);
    if (m_currentFd < 0)
        KSystemError("socket() failed for " + m_hostname + ".", errno);

    int err = ::connect(m_currentFd, (struct sockaddr *)&addr, sizeof(addr));
    if (err != 0) {
        close();
        throw KSystemError("connect() failed for " + m_hostname + ".", errno);
    }

    return m_currentFd;
}

// -----------------------------------------------------------------------------
void Socket::close()
    throw ()
{
    Debug::debug()->trace("Socket::close(): m_currentFd=%d", m_currentFd);

    if (m_currentFd > 0) {
        ::close(m_currentFd);
        m_currentFd = -1;
    }
}

/* -------------------------------------------------------------------------- */
int Socket::getCurrentFd() const
    throw ()
{
    return m_currentFd;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

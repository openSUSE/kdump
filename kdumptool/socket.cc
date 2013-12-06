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
#include <sstream>

#include "global.h"
#include "socket.h"
#include "debug.h"

using std::memcpy;

//{{{ Socket -------------------------------------------------------------------

Socket::Socket(const char *address, int port,
	       Socket::SocketType socketType, Socket::Family family)
    throw ()
    : m_currentFd(-1), m_hostname(address), m_port(port),
      m_socketType(socketType), m_family(family)
{
    Debug::debug()->trace("Socket(%s, %d, %d, %d)",
			  address, port, socketType, family);
}

// -----------------------------------------------------------------------------
Socket::~Socket()
    throw ()
{
    Debug::debug()->trace("Socket::~Socket()");
    close();
}

// -----------------------------------------------------------------------------
static int systemFamily(enum Socket::Family family)
{
    switch (family) {
    case Socket::SF_IPv4:
        return AF_INET;

    case Socket::SF_IPv6:
        return AF_INET6;

    case Socket::SF_ANY:
    default:
        return AF_UNSPEC;
    }
}

// -----------------------------------------------------------------------------
static int systemSocketType(enum Socket::SocketType socketType)
{
    switch (socketType) {
    case Socket::ST_UDP:
        return SOCK_DGRAM;

    case Socket::ST_TCP:
    default:
        return SOCK_STREAM;
    }
}

// -----------------------------------------------------------------------------
int Socket::connect()
    throw (KError)
{
    struct addrinfo hints, *res, *aip;
    int n;

    Debug::debug()->trace("Socket::connect()");

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = systemFamily(m_family);
    hints.ai_socktype = systemSocketType(m_socketType);

    std::ostringstream service;
    service << m_port;
    n = getaddrinfo(m_hostname.c_str(), service.str().c_str(), &hints, &res);
    if (n < 0)
        throw KError("getaddrinfo() failed for " + m_hostname + ".");

    for (aip = res; aip; aip = aip->ai_next) {
        Debug::debug()->trace("Socket::connect(): socket(%d, %d, %d)",
                              aip->ai_family, aip->ai_socktype,
                              aip->ai_protocol);

        m_currentFd = socket(aip->ai_family, aip->ai_socktype,
                             aip->ai_protocol);
        if (m_currentFd < 0) {
            Debug::debug()->dbg("socket() failed.");
            continue;
        }

        if (::connect(m_currentFd, res->ai_addr, res->ai_addrlen) == 0)
            break;

        Debug::debug()->dbg("connect() failed.");
        close();
    }

    freeaddrinfo(res);

    if (!aip)
        throw KError("connect() failed for " + m_hostname + ".");

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

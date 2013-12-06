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
#ifndef SOCKET_H
#define SOCKET_H

#include <string>
#include <stdint.h>

#include "global.h"
#include "optionparser.h"
#include "subcommand.h"

//{{{ Socket -------------------------------------------------------------------

/**
 * Represents a socket (IPv4 or IPv6).
 */
class Socket {

    public:
        /**
         * The type of the socket. Currently, all combinations of
         * TCP and UDP over IPv4 or IPv6 are supported.
         */
        enum SocketType {
            ST_TCP,             /**< TCP connections */
            ST_UDP,             /**< UDP connections */
        };

        /**
         * The protocol family.
         */
        enum Family {
            SF_ANY,             /**< Any protocol */
            SF_IPv4,            /**< IPv4 only */
            SF_IPv6,            /**< IPv6 only */
        };

        /**
         * Some default ports.
         */
        enum DefaultPort {
            DP_SSH = 22 /**< Secure Shell */
        };

    public:

        /**
         * Creates a new socket.
         *
         * @param[in] address the IP address or the hostname
         * @param[in] service the service to connect (or port as string)
         * @param[in] socketType the socket type (TCP or UDP)
         * @param[in] family protocol family (IPv4, IPv6, or both)
         */
        Socket(const std::string &address, const std::string &service,
               SocketType socketType, Family family = SF_ANY)
        throw ();

        /**
         * Creates a new socket.
         *
         * @param[in] address the IP address or the hostname
         * @param[in] port the port number to connect
         * @param[in] socketType the socket type (TCP or UDP)
         * @param[in] family protocol family (IPv4, IPv6, or both)
         */
        Socket(const std::string &address, int port,
               SocketType socketType, Family family = SF_ANY)
        throw ();

        /**
         * Destructor. Closes the connection.
         */
        virtual ~Socket()
        throw ();

        /**
         * Establishes the connection and returns the file descriptor.
         *
         * @return the file descirptor which can be used by read(),
         *         write(), recv() and send().
         * @exception KError if the parsing of that URL fails
         */
        int connect()
        throw (KError);

        /**
         * Returns the current file descriptor (form the last Socket::connect()
         * call or -1 of there's no open connection.
         *
         * @return the file descriptor
         */
        int getCurrentFd() const
        throw ();

        /**
         * Closes the connection.
         */
        void close()
        throw ();

    private:
        int m_currentFd;
        std::string m_hostname;
        std::string m_service;
        SocketType m_socketType;
        Family m_family;

        void setHostname(const std::string &address)
        throw();
};

//}}}

#endif /* SOCKET_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

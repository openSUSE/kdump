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
 * Represents a socket (IPv4).
 */
class Socket {

    public:
        /**
         * The type of the socket. Currently, IPv4/TCP and IPv4/UDP are
         * supported.
         */
        enum Type {
            ST_TCP,     /**< IPv4 and TCP */
            ST_UCP      /**< IPv4 and UDP */
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
         * @param[in] port the port to connect
         * @param[in] type the socket type
         */
        Socket(const char *address, int port, Type type)
        throw ();

        /**
         * Desctructor. Closes the connection.
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
        int m_port;
        Type m_connectionType;
};

//}}}

#endif /* SOCKET_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

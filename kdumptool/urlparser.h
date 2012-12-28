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
#ifndef URLPARSER_H
#define URLPARSER_H

#include <string>
#include <vector>
#include <stdint.h>

#include "global.h"
#include "optionparser.h"
#include "subcommand.h"

//{{{ URLParser ----------------------------------------------------------------

/**
 * Parse a URL which specifies the location where to save the dump.
 */
class URLParser {

    public:
        /**
         * Protocol used for file transfers.
         */
        enum Protocol {
            PROT_FILE,          /**< normal file transfers */
            PROT_FTP,           /**< File Transfer Protocol */
            PROT_SFTP,          /**< Secure File Transfer Protocol */
            PROT_NFS,           /**< Network File System */
            PROT_CIFS           /**< Common Internet File System (SMB) */
        };

    public:

        /**
         * Creates a new URLParser.
         *
         * @param[in] url the URL to parse
         * @exception KError if the parsing of that URL fails
         */
        URLParser(const std::string &url)
        throw (KError);

        /**
         * Desctructor.
         */
        virtual ~URLParser () {}

        /**
         * Returns the protocol of the URL, i.e. PROT_FILE, @c PROT_FTP,
         * @c PROT_SFTP, @c PROT_NFS or @c PROT_CIFS.
         *
         * @return the protocol
         */
        Protocol getProtocol() const
        throw ();

        /**
         * Returns the protocol as string.
         *
         * @return the protocol as string, i.e. @c file, @c ftp, @c sftp,
         * @c nfs or @c cifs.
         */
        std::string getProtocolAsString() const
        throw ();

        /**
         * Returns the username as string. Returns an empty string if
         * the property "username" is not applicable (i.e. for NFS).
         *
         * @return the username
         */
        std::string getUsername() const
        throw ();

        /**
         * Returns the password as string. Returns an empty string if
         * the property "password" is not applicable (i.e. for NFS).
         *
         * @return the password
         */
        std::string getPassword() const
        throw ();

        /**
         * Returns the hostname. Returns an empty string if no hostname
         * is applicable for that property.
         *
         * @return the hostname as string
         */
        std::string getHostname() const
        throw ();

        /**
         * Returns the port or -1 if the port is not applicable for that
         * protocol (i.e. for local files).
         *
         * @return the port
         */
        int getPort() const
        throw ();

        /**
         * Returns the path. For CIFS, it returns the path without the "share"
         * part.
         *
         * @return the path
         */
        std::string getPath() const
        throw ();

        /**
         * Returns the URL.
         *
         * @return the URL as string
         */
        std::string getURL() const
        throw ();

    public:

        /**
         * Tries to get the protocol from the specified string.
         *
         * @param[in] the protocol as string
         * @return the protocol as constant
         *
         * @exception KError if the protocol does not exist
         */
        static Protocol string2protocol(const std::string &protocol)
        throw (KError);

        /**
         * Convers a protocol constant to a readable string.
         *
         * @param[in] the protocol as constant
         * @return the protocol as string
         *
         * @exception KError if the protocol constant is invalid
         */
        static std::string protocol2string(Protocol protocol)
        throw (KError);

    protected:

        void parseCifsUrl(const std::string &partUrl)
        throw (KError);

        void parseNFSUrl(const std::string &partUrl)
        throw (KError);

        void parseUserPassHostPort(const std::string &userpasshostport)
        throw (KError);

        void parseFTPUrl(const std::string &partUrl)
        throw (KError);

    private:
        std::string m_url;
        Protocol m_protocol;
        std::string m_hostname;
        std::string m_password;
        std::string m_username;
        int m_port;
        std::string m_path;
        std::string m_share;
};

//}}}

#endif /* URLPARSER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

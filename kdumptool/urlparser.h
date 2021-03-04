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
#include "stringutil.h"

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
            PROT_SSH,           /**< Secure Shell */
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
        URLParser(const std::string &url);

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
        Protocol getProtocol() const;

        /**
         * Returns the protocol as string.
         *
         * @return the protocol as string, i.e. @c file, @c ftp, @c sftp,
         * @c nfs or @c cifs.
         */
        std::string getProtocolAsString() const;

        /**
         * Returns the username as string. Returns an empty string if
         * the property "username" is not applicable (i.e. for NFS).
         *
         * @return the username
         */
        std::string getUsername() const;

        /**
         * Returns the password as string. Returns an empty string if
         * the property "password" is not applicable (i.e. for NFS).
         *
         * @return the password
         */
        std::string getPassword() const;

        /**
         * Returns the hostname. Returns an empty string if no hostname
         * is applicable for that property.
         *
         * @return the hostname as string
         */
        std::string getHostname() const;

        /**
         * Returns the port or -1 if the port is not applicable for that
         * protocol (i.e. for local files).
         *
         * @return the port
         */
        int getPort() const;

        /**
         * Returns the path. For CIFS, it returns the path without the "share"
         * part.
         *
         * @return the path
         */
        std::string getPath() const;

        /**
         * Returns the URL.
         *
         * @return the URL as string
         */
        std::string getURL() const;

    public:

        /**
         * Tries to get the protocol from the specified string.
         *
         * @param[in] the protocol as string
         * @return the protocol as constant
         *
         * @exception KError if the protocol does not exist
         */
        static Protocol string2protocol(const std::string &protocol);

        /**
         * Convers a protocol constant to a readable string.
         *
         * @param[in] the protocol as constant
         * @return the protocol as string
         *
         * @exception KError if the protocol constant is invalid
         */
        static std::string protocol2string(Protocol protocol);

    private:
	std::string m_url;
        Protocol m_protocol;
        KString m_hostname;
        KString m_password;
        KString m_username;
        int m_port;
        KString m_path;

	/**
	 * Extract scheme from a string
	 *
	 * @param[in,out] it starting position (updated if scheme is found)
	 * @param[in] end end of input string
	 */
	std::string extractScheme(std::string::iterator &it,
				  const std::string::const_iterator &end);

	/**
	 * Extract authority from a string
	 *
	 * @param[in,out] it starting position (updated if authority is found)
	 * @param[in] end end of input string
	 */
	std::string extractAuthority(std::string::iterator &it,
				     const std::string::const_iterator &end);
};

//}}}

#endif /* URLPARSER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

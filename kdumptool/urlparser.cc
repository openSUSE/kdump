/*
 * (c) 2008, Bernhard Walle <bwalle@suse.de>, SUSE LINUX Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your opti) any later version.
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
#include <cstring>
#include <strings.h>
#include <stdint.h>

#include "global.h"
#include "debug.h"
#include "urlparser.h"
#include "stringutil.h"

using std::string;

#define FTP_DEFAULT_USER   "anonymous"
#define SFTP_DEFAULT_USER  "root"

//{{{ URLParser ----------------------------------------------------------------

// -----------------------------------------------------------------------------
URLParser::Protocol URLParser::string2protocol(const string &protocol)
    throw (KError)
{
    if (protocol.size() == 0)
        throw KError("Empty protocols are not allowed.");

    if (strcasecmp(protocol.c_str(), "file") == 0)
        return PROT_FILE;
    else if (strcasecmp(protocol.c_str(), "ftp") == 0)
        return PROT_FTP;
    else if (strcasecmp(protocol.c_str(), "scp") == 0)
        return PROT_SFTP;
    else if (strcasecmp(protocol.c_str(), "sftp") == 0)
        return PROT_SFTP;
    else if (strcasecmp(protocol.c_str(), "scp") == 0)
        return PROT_SFTP;
    else if (strcasecmp(protocol.c_str(), "ssh") == 0)
        return PROT_SSH;
    else if (strcasecmp(protocol.c_str(), "nfs") == 0)
        return PROT_NFS;
    else if (strcasecmp(protocol.c_str(), "cifs") == 0)
        return PROT_CIFS;
    else if (strcasecmp(protocol.c_str(), "smb") == 0)
        return PROT_CIFS;
    else
        throw KError("Protocol " + protocol + " is invalid.");
}

// -----------------------------------------------------------------------------
string URLParser::protocol2string(URLParser::Protocol protocol)
    throw (KError)
{
    switch (protocol) {
        case PROT_FILE:
            return "file";
        case PROT_FTP:
            return "ftp";
        case PROT_SFTP:
            return "sftp";
        case PROT_SSH:
            return "ssh";
        case PROT_NFS:
            return "nfs";
        case PROT_CIFS:
            return "cifs";
        default:
            throw KError("Invalid protocol constant: " +
                Stringutil::number2string(protocol) + ".");
    }
}

// -----------------------------------------------------------------------------
URLParser::URLParser(const std::string &url)
    throw (KError)
    : m_url(url), m_port(-1)
{
    Debug::debug()->trace("URLParser::URLParser(%s)", url.c_str());

    if (url.size() == 0)
        throw KError("URL must be longer than 0 characters.");

    // local files that don't have URL syntax
    // we support that for backward-compatibility
    if (url[0] == '/') {
        m_protocol = PROT_FILE;
        m_path = url;
        return;
    }

    //
    // get the protocol
    //

    string::size_type first_colon = url.find(':');
    if (first_colon == string::npos)
        throw KError("The URL does not contain any protocol.");
    string proto_part = url.substr(0, first_colon);
    Debug::debug()->trace("Setting protocol to %s", proto_part.c_str());
    m_protocol = string2protocol(proto_part);

    if (url.size() < (first_colon + 3) ||
            url[first_colon+1] != '/' ||
            url[first_colon+2] != '/')
        throw KError("protocol: must be followed by '//'.");

    //
    // call the parse methods matching for the protocol
    //

    if (m_protocol == PROT_FILE) {
        m_path = url.substr(first_colon+3);
    } else if (m_protocol == PROT_NFS) {
        parseNFSUrl(url.substr(first_colon+3));
    } else if (m_protocol == PROT_SFTP || m_protocol == PROT_SSH ||
	       m_protocol == PROT_FTP || m_protocol == PROT_CIFS) {
        parseFTPUrl(url.substr(first_colon+3));
    } else
        throw KError("Invalid protocol: " +
            Stringutil::number2string(m_protocol) + ".");

    Debug::debug()->dbg("URL parsed as: protocol=%s, host=%s, port=%d, "
        "username=%s, password=%s, path=%s",
        getProtocolAsString().c_str(),
        getHostname().c_str(), getPort(), getUsername().c_str(),
        getPassword().c_str(), getPath().c_str());

}

// -----------------------------------------------------------------------------
void URLParser::parseUserPassHostPort(const string &userpasshostport)
    throw (KError)
{
    // now scan for an '@' to separate userhost from hostport
    string::size_type last_at = userpasshostport.rfind('@');
    string userhost;
    string hostport;
    if (last_at == string::npos) {
        hostport = userpasshostport;

        switch (m_protocol) {
            case PROT_FTP:
                m_username = FTP_DEFAULT_USER;
                break;

            case PROT_SFTP:
	    case PROT_SSH:
                m_username = SFTP_DEFAULT_USER;
                break;

            default:
                // do nothing but make the compiler happy
                break;
        }

    } else {
        string userpass = userpasshostport.substr(0, last_at);
        hostport = userpasshostport.substr(last_at+1);

        // now separate user and passwort
        string::size_type firstcolon = userpass.find(':');
        if (firstcolon != string::npos) {
            m_username = userpass.substr(0, firstcolon);
            m_password = userpass.substr(firstcolon+1);
        } else
            m_username = userpass;
    }

    // look for a literal IPv6 addresses
    string::size_type last_colon = hostport.rfind(':');
    if (hostport[0] == '[') {
        string::size_type bracket = hostport.find(']');
        if (bracket != string::npos && bracket > last_colon)
            last_colon = string::npos;
    }

    // and separate host and port
    if (last_colon != string::npos) {
        m_hostname = hostport.substr(0, last_colon);
        string portstr = hostport.substr(last_colon+1);
        if (portstr.size() > 0)
            m_port = Stringutil::string2number(portstr);
    } else
        m_hostname = hostport;
}

// -----------------------------------------------------------------------------
void URLParser::parseNFSUrl(const string &partUrl)
    throw (KError)
{
    Debug::debug()->trace("URLParser::parseNFSUrl(%s)", partUrl.c_str());

    // look for the first '/'
    string::size_type first_slash = partUrl.find('/');
    if (first_slash == string::npos)
        throw KError("NFS URL must contain at least one '/'.");

    m_hostname = partUrl.substr(0, first_slash);

    string::size_type last_colon = m_hostname.rfind(':');
    if (m_hostname[0] == '[') {
        string::size_type bracket = m_hostname.find(']');
        if (bracket != string::npos && bracket > last_colon)
            last_colon = string::npos;
    }
    if (last_colon != string::npos) {
        string hostport = m_hostname;
        m_hostname = hostport.substr(0, last_colon);
        string portstring = hostport.substr(last_colon+1);
        if (portstring.size() > 0)
            m_port = Stringutil::string2number(hostport.substr(last_colon+1));
    }

    m_path = partUrl.substr(first_slash);
}

// -----------------------------------------------------------------------------
void URLParser::parseFTPUrl(const string &partUrl)
    throw (KError)
{
    Debug::debug()->trace("URLParser::parseFTPUrl(%s)", partUrl.c_str());

    // look for the first '/' to separate the host name part from the
    // path name
    string::size_type first_slash = partUrl.find('/');
    if (first_slash == string::npos)
        throw KError(getProtocolAsString() +
            " URL must contain at least one '/'.");

    parseUserPassHostPort(partUrl.substr(0, first_slash));

    // and the rest is the path
    m_path = partUrl.substr(first_slash);
}

// -----------------------------------------------------------------------------
URLParser::Protocol URLParser::getProtocol() const
    throw ()
{
    return m_protocol;
}

// -----------------------------------------------------------------------------
string URLParser::getProtocolAsString() const
    throw ()
{
    return protocol2string(m_protocol);
}

// -----------------------------------------------------------------------------
string URLParser::getUsername() const
    throw ()
{
    return m_username;
}

// -----------------------------------------------------------------------------
string URLParser::getPassword() const
    throw ()
{
    return m_password;
}

// -----------------------------------------------------------------------------
string URLParser::getHostname() const
    throw ()
{
    return m_hostname;
}

// -----------------------------------------------------------------------------
int URLParser::getPort() const
    throw ()
{
    return m_port;
}

// -----------------------------------------------------------------------------
string URLParser::getPath() const
    throw ()
{
    return m_path;
}

// -----------------------------------------------------------------------------
string URLParser::getURL() const
    throw ()
{
    return m_url;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

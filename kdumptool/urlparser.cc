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
string URLParser::extractScheme(string::iterator &it,
				const string::const_iterator &end)
{
    string::iterator const start = it;

    if (it == m_url.end() || !isalpha(*it))
	return string();

    do {
	++it;
	if (it == m_url.end()) {
	    it = start;
	    return string();
	}
    } while (*it != ':' && *it != '/' && *it != '?' && *it != '#');

    if (*it != ':') {
	it = start;
	return string();
    }

    return string(start, it++);
}

// -----------------------------------------------------------------------------
string URLParser::extractAuthority(string::iterator &it,
				   const string::const_iterator &end)
{
    if (end - it < 2 || it[0] != '/' || it[1] != '/')
	return string();

    it += 2;
    string::iterator const start = it;
    while (*it != '/' && *it != '?' && *it != '#')
	++it;

    return string(start, it);
}

// -----------------------------------------------------------------------------
URLParser::URLParser(const std::string &url)
    : m_url(url), m_port(-1)
{
    Debug::debug()->trace("URLParser::URLParser(%s)", url.c_str());

    string::iterator it = m_url.begin();

    //
    // extract the three main URL componenets
    //
    string scheme = extractScheme(it, m_url.end());
    string authority = extractAuthority(it, m_url.end());
    m_path.assign(it, m_url.end());
    // TODO: query and fragment are not handled

    if (m_path.empty() || m_path[0] != '/')
        throw KError("URLParser: Only absolute paths are supported.");

    //
    // parse the authority part
    //

    // Look for (optional) port
    it = authority.end();
    while (it != authority.begin() && isdigit(it[-1]))
	--it;
    if (it != authority.begin() && it[-1] == ':') {
	KString port(it, authority.end());
	authority.resize(it - authority.begin() - 1);

        if (port.size() > 0)
            m_port = port.asInt();
    }

    // look for userinfo
    string::size_type last_at = authority.rfind('@');
    if (last_at != string::npos) {
        m_hostname = authority.substr(last_at+1);
	authority.resize(last_at);

        // now separate user and password
        string::size_type first_colon = authority.find(':');
        if (first_colon != string::npos) {
            m_username = authority.substr(0, first_colon);
            m_password = authority.substr(first_colon+1);
        } else
            m_username = authority;
    } else
        m_hostname = authority;

    //
    // undo percent encoding
    //
    m_hostname.decodeURL();
    m_password.decodeURL();
    m_username.decodeURL();
    m_path.decodeURL();

    //
    // guess the scheme, if omitted
    //
    if (scheme.empty()) {
	if (m_hostname.empty() || m_hostname == "localhost") {
	    m_protocol = PROT_FILE;
	    Debug::debug()->trace("URL looks like a local file");
	} else {
	    m_protocol = PROT_NFS;
	    Debug::debug()->trace("URL looks like a remote file");
	}
    } else {
	Debug::debug()->trace("Scheme explicitly set to %s", scheme.c_str());
	m_protocol = string2protocol(scheme);
    }

    //
    // protocol-specific defaults
    //

    if (m_protocol == PROT_FILE && m_hostname == "localhost")
	m_hostname.clear();

    if (m_username.empty()) {
	if (m_protocol == PROT_FTP)
	    m_username = FTP_DEFAULT_USER;
	else if (m_protocol == PROT_SFTP || m_protocol == PROT_SSH)
	    m_username = SFTP_DEFAULT_USER;
    }

    //
    // sanity checks
    //
    if (m_protocol == PROT_FILE && !m_hostname.empty())
	throw KError("File protocol cannot specify a remote host");

    Debug::debug()->dbg("URL parsed as: protocol=%s, host=%s, port=%d, "
        "username=%s, password=%s, path=%s",
        getProtocolAsString().c_str(),
        getHostname().c_str(), getPort(), getUsername().c_str(),
        getPassword().c_str(), getPath().c_str());

}

// -----------------------------------------------------------------------------
URLParser::Protocol URLParser::getProtocol() const
{
    return m_protocol;
}

// -----------------------------------------------------------------------------
string URLParser::getProtocolAsString() const
{
    return protocol2string(m_protocol);
}

// -----------------------------------------------------------------------------
string URLParser::getUsername() const
{
    return m_username;
}

// -----------------------------------------------------------------------------
string URLParser::getPassword() const
{
    return m_password;
}

// -----------------------------------------------------------------------------
string URLParser::getHostname() const
{
    return m_hostname;
}

// -----------------------------------------------------------------------------
int URLParser::getPort() const
{
    return m_port;
}

// -----------------------------------------------------------------------------
string URLParser::getPath() const
{
    return m_path;
}

// -----------------------------------------------------------------------------
string URLParser::getURL() const
{
    return m_url;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

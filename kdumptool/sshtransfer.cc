/*
 * (c) 2015, Petr Tesarik <ptesarik@suse.cz>, SUSE LINUX Products GmbH
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
#include <iostream>
#include <string>
#include <cstdlib>
#include <cerrno>
#include <stdint.h>

#if HAVE_LIBSSH2
#   include <libssh2.h>
#   include <libssh2_sftp.h>
#endif

#include "global.h"
#include "debug.h"
#include "configuration.h"
#include "fileutil.h"
#include "dataprovider.h"
#include "process.h"
#include "socket.h"
#include "sshtransfer.h"

using std::string;
using std::cerr;
using std::endl;

//{{{ SSHTransfer -------------------------------------------------------------

/* -------------------------------------------------------------------------- */
SSHTransfer::SSHTransfer(const RootDirURLVector &urlv,
			 const std::string &subdir)
    throw (KError)
    : URLTransfer(urlv, subdir)
{
    if (urlv.size() > 1)
	cerr << "WARNING: First dump target used; rest ignored." << endl;
    const RootDirURL &target = urlv.front();

    Debug::debug()->trace("SSHTransfer::SSHTransfer(%s)",
			  target.getURL().c_str());

    string remote;
    FilePath fp = target.getPath();
    fp.appendPath(getSubDir());
    remote.assign("mkdir -p ").append(fp);

    SubProcess p;
    p.spawn("ssh", makeArgs(remote));
    int status = p.wait();
    if (status != 0)
	throw KError("SSHTransfer::SSHTransfer: ssh command failed"
		     " with status " + Stringutil::number2string(status));
}

/* -------------------------------------------------------------------------- */
SSHTransfer::~SSHTransfer()
    throw ()
{
    Debug::debug()->trace("SSHTransfer::~SSHTransfer()");
}

/* -------------------------------------------------------------------------- */
void SSHTransfer::perform(DataProvider *dataprovider,
			  const StringVector &target_files,
			  bool *directSave)
    throw (KError)
{
    Debug::debug()->trace("SSHTransfer::perform(%p, [ \"%s\"%s ])",
	dataprovider, target_files.front().c_str(),
	target_files.size() > 1 ? ", ..." : "");

    bool prepared = false;
    if (directSave)
        *directSave = false;

    RootDirURLVector &urlv = getURLVector();
    const RootDirURL &target = urlv.front();

    FilePath fp = target.getPath();
    fp.appendPath(getSubDir()).appendPath(target_files.front());

    string remote;
    remote.assign("dd of=").append(fp).append("-incomplete");
    remote.append(" && mv ").append(fp).append("-incomplete ").append(fp);
    Debug::debug()->dbg("Remote command: %s", remote.c_str());

    SubProcess p;
    p.setPipeDirection(STDIN_FILENO, SubProcess::ParentToChild);
    p.spawn("ssh", makeArgs(remote));

    int fd = p.getPipeFD(STDIN_FILENO);
    try {
        dataprovider->prepare();
        prepared = true;

        while (true) {
            size_t read_data = dataprovider->getData(m_buffer, BUFSIZ);

            // finished?
            if (read_data == 0)
                break;

	    char *p = m_buffer;
	    while (read_data) {
		ssize_t ret = write(fd, p, read_data);

		if (ret < 0)
		    throw KSystemError("SSHTransfer::perform: write failed",
				       errno);
		read_data -= ret;
		p += ret;
	    }
        }
    } catch (...) {
	close(fd);
        if (prepared)
            dataprovider->finish();
        throw;
    }
    close(fd);

    dataprovider->finish();
    int status = p.wait();
    if (status != 0)
	throw KError("SSHTransfer::perform: ssh command failed"
		     " with status " + Stringutil::number2string(status));
}

StringVector SSHTransfer::makeArgs(std::string const &remote)
{
    const RootDirURL &target = getURLVector().front();
    StringVector ret;

    ret.push_back("-F");
    ret.push_back("/kdump/.ssh/config");

    ret.push_back("-l");
    ret.push_back(target.getUsername());

    int port = target.getPort();
    if (port != -1) {
	ret.push_back("-p");
	ret.push_back(Stringutil::number2string(port));
    }

    ret.push_back(target.getHostname());
    ret.push_back(remote);

    return ret;
}

//}}}
//{{{ SFTPPacket ---------------------------------------------------------------

/* -------------------------------------------------------------------------- */
SFTPPacket::SFTPPacket(void)
    : m_vector(sizeof(uint32_t))
{
}

/* -------------------------------------------------------------------------- */
void SFTPPacket::addInt32(unsigned long val)
{
    int i;
    for (i = sizeof(uint32_t) - 1; i >= 0; --i)
	m_vector.push_back((val >> (i*8)) & 0xff);
}

/* -------------------------------------------------------------------------- */
ByteVector const &SFTPPacket::update(void)
{
    uint_fast32_t len = m_vector.size() - sizeof(uint32_t);
    m_vector[0] = (len >> 24) & 0xff;
    m_vector[1] = (len >> 16) & 0xff;
    m_vector[2] = (len >>  8) & 0xff;
    m_vector[3] = (len      ) & 0xff;
    return m_vector;
}

//}}}
//{{{ SFTPTransfer -------------------------------------------------------------

#if HAVE_LIBSSH2

/* -------------------------------------------------------------------------- */
SFTPTransfer::SFTPTransfer(const RootDirURLVector &urlv,
			   const std::string &subdir)
    throw (KError)
    : URLTransfer(urlv, subdir),
      m_sshSession(NULL), m_sftp(NULL), m_socket(NULL)
{
    if (urlv.size() > 1)
	cerr << "WARNING: First dump target used; rest ignored." << endl;
    const RootDirURL &parser = urlv.front();

    Debug::debug()->trace("SFTPTransfer::SFTPTransfer(%s)",
			  parser.getURL().c_str());

    m_sshSession = libssh2_session_init();
    if (!m_sshSession)
        throw KError("libssh2_session_init() failed.");

    // set blocking
    libssh2_session_set_blocking(m_sshSession, 1);

    // get the correct port
    int port = parser.getPort();
    if (port <= 0)
        port = Socket::DP_SSH;

    // create the socket and connect
    m_socket = new Socket(parser.getHostname(), port, Socket::ST_TCP);
    int fd = m_socket->connect();

    // start it up
    int ret = libssh2_session_startup(m_sshSession, fd);
    if (ret != 0) {
        close();
        throw KError("libssh2_session_startup() failed with "+
            Stringutil::number2string(ret) +".");
    }

    // get the hostkey fingerprints
    const char *hostsha1 =
	libssh2_hostkey_hash(m_sshSession, LIBSSH2_HOSTKEY_HASH_SHA1);
    Debug::debug()->info
	("SSH SHA1 fingerprint: %s",
	 Stringutil::bytes2hexstr(hostsha1, SHA1SUM_LENGTH, true).c_str());

    const char *hostmd5 =
	libssh2_hostkey_hash(m_sshSession, LIBSSH2_HOSTKEY_HASH_MD5);
    Debug::debug()->info
	("SSH MD5 fingerprint: %s",
	 Stringutil::bytes2hexstr(hostmd5, MD5SUM_LENGTH, true).c_str());

#if HAVE_LIBSSL
    // check the fingerprints if possible
    Configuration *config = Configuration::config();
    const string &hostkey = config->KDUMP_HOST_KEY.value();

    if (!hostkey.empty() && hostkey != "*") {
	char expectmd5[MD5SUM_LENGTH];
	char expectsha1[SHA1SUM_LENGTH];
	Stringutil::digest_base64(hostkey.c_str(), hostkey.size(),
				  expectmd5, expectsha1);

	if (memcmp(hostsha1, expectsha1, SHA1SUM_LENGTH))
	    throw KError("Target host key SHA1 fingerprint mismatch!");
	Debug::debug()->info("SHA1 fingerprint matches");

	if (memcmp(hostmd5, expectmd5, MD5SUM_LENGTH))
	    throw KError("Target host key MD5 fingerprint mismatch!");
	Debug::debug()->info("MD5 fingerprint matches");
    } else
	cerr << "WARNING: SSH host key accepted without checking!" << endl;
#endif	// HAVE_LIBSSL

    // SSH authentication
    bool authenticated = false;

    // username and password
    string username = parser.getUsername();
    string password = parser.getPassword();

    // public and private key
    string homedir = getenv("HOME");
    FilePath pubkey;
    FilePath privkey;

    // DSA
    (pubkey = homedir).appendPath(".ssh").appendPath("id_dsa.pub");
    (privkey = homedir).appendPath(".ssh").appendPath("id_dsa");
    if (!authenticated && pubkey.exists() && privkey.exists()) {
        Debug::debug()->dbg("Using private key %s and public key %s",
                privkey.c_str(), pubkey.c_str());

        ret = libssh2_userauth_publickey_fromfile(m_sshSession,
                username.c_str(), pubkey.c_str(), privkey.c_str(),
                password.c_str());
        if (ret == 0)
            authenticated = true;
        else
            Debug::debug()->dbg("id_dsa: "
                "libssh2_userauth_publickey_fromfile() failed with "+
                Stringutil::number2string(ret) + ".");
    }

    // RSA
    (pubkey = homedir).appendPath(".ssh").appendPath("id_rsa.pub");
    (privkey = homedir).appendPath(".ssh").appendPath("id_rsa");
    if (!authenticated && pubkey.exists() && privkey.exists()) {
        Debug::debug()->dbg("Using private key %s and public key %s",
                privkey.c_str(), pubkey.c_str());

        ret = libssh2_userauth_publickey_fromfile(m_sshSession,
                username.c_str(), pubkey.c_str(), privkey.c_str(),
                password.c_str());
        if (ret == 0)
            authenticated = true;
        else
            Debug::debug()->dbg("id_rsa: "
                "libssh2_userauth_publickey_fromfile() failed with "+
                Stringutil::number2string(ret) + ".");
    }

    // password
    if (!authenticated) {
        Debug::debug()->dbg("Using password auth");

        ret = libssh2_userauth_password(m_sshSession, username.c_str(),
            password.c_str());
        if (ret == 0)
            authenticated = true;
        else
            Debug::debug()->dbg("libssh2_userauth_password() failed with "+
                Stringutil::number2string(ret) + ".");
    }

    if (!authenticated) {
        close();
        throw KError("SSH authentication failed.");
    }

    // SFTP session
    m_sftp = libssh2_sftp_init(m_sshSession);
    if (!m_sftp) {
        close();
        throw KError("libssh2_sftp_init() failed with "+
            Stringutil::number2string(ret) + ".");
    }

    FilePath fp = parser.getPath();
    mkdir(fp.appendPath(subdir), true);
}

/* -------------------------------------------------------------------------- */
SFTPTransfer::~SFTPTransfer()
    throw ()
{
    Debug::debug()->trace("SFTPTransfer::~SFTPTransfer()");

    close();
}

// -----------------------------------------------------------------------------
bool SFTPTransfer::exists(const string &file)
    throw (KError)
{
    Debug::debug()->trace("SFTPTransfer::exists(%s)", file.c_str());

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int ret = libssh2_sftp_stat(m_sftp, file.c_str(), &attrs);

    if (ret != 0) {
        int errorcode = libssh2_sftp_last_error(m_sftp);
        if (errorcode == LIBSSH2_FX_NO_SUCH_FILE)
            return false;
        else
            throw KSFTPError("libssh2_sftp_stat on " + file + " failed.",
                errorcode);
    }

    return true;
}

// -----------------------------------------------------------------------------
void SFTPTransfer::mkdir(const FilePath &dir, bool recursive)
    throw (KError)
{
    Debug::debug()->trace("SFTPTransfer::mkdir(%s, %d)",
        dir.c_str(), int(recursive));

    if (!recursive) {
        if (!exists(dir)) {
            int ret = libssh2_sftp_mkdir(m_sftp, dir.c_str(), 0755);
            if (ret != 0)
                throw KSFTPError("mkdir of " + dir + " failed.",
                    libssh2_sftp_last_error(m_sftp));
        }
    } else {
        string directory = dir;

        // remove trailing '/' if there are any
        while (directory[directory.size()-1] == '/')
            directory = directory.substr(0, directory.size()-1);

        string::size_type current_slash = 0;

        while (true) {
            current_slash = directory.find('/', current_slash+1);
            if (current_slash == string::npos) {
                mkdir(directory, false);
                break;
            }

            mkdir(directory.substr(0, current_slash), false);
        }
    }
}

/* -------------------------------------------------------------------------- */
void SFTPTransfer::perform(DataProvider *dataprovider,
                           const StringVector &target_files,
                           bool *directSave)
    throw (KError)
{
    Debug::debug()->trace("SFTPTransfer::perform(%p, [ \"%s\"%s ])",
	dataprovider, target_files.front().c_str(),
	target_files.size() > 1 ? ", ..." : "");

    bool prepared = false;
    if (directSave)
        *directSave = false;

    RootDirURLVector &urlv = getURLVector();
    const RootDirURL &parser = urlv.front();

    LIBSSH2_SFTP_HANDLE  *handle = NULL;
    FilePath file = parser.getPath();
    file.appendPath(getSubDir()).appendPath(target_files.front());

    Debug::debug()->dbg("Using target file %s.", file.c_str());

    try {
        handle = libssh2_sftp_open(m_sftp, file.c_str(),
            LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT|LIBSSH2_FXF_TRUNC, 0644);
        if (!handle)
            throw KSFTPError("Cannot create file " + file + " remotely.",
                libssh2_sftp_last_error(m_sftp));

        dataprovider->prepare();
        prepared = true;

        while (true) {
            size_t read_data = dataprovider->getData(m_buffer, BUFSIZ);

            // finished?
            if (read_data == 0)
                break;

            size_t ret = libssh2_sftp_write(handle, m_buffer, read_data);
            if (ret != read_data)
                throw KSFTPError("SFTPTransfer::perform: "
                    "libssh2_sftp_write() failed.",
                    libssh2_sftp_last_error(m_sftp));
        }
    } catch (...) {
        if (handle)
            libssh2_sftp_close(handle);
        close();
        if (prepared)
            dataprovider->finish();
        throw;
    }

    if (handle)
        libssh2_sftp_close(handle);
    dataprovider->finish();
}


/* -------------------------------------------------------------------------- */
void SFTPTransfer::close()
    throw ()
{
    Debug::debug()->trace("SFTPTransfer::close()");

    if (m_sshSession) {
        libssh2_session_disconnect(m_sshSession, "Normal Shutdown.");
        libssh2_session_free(m_sshSession);
        m_sshSession = NULL;
    }
    delete m_socket;
    m_socket = NULL;
}

#endif // HAVE_LIBSSH2

//}}}

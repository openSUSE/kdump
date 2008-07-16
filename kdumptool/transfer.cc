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
#include <cstdio>
#include <cstdarg>
#include <cerrno>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

#include <curl/curl.h>
#include <libssh2.h>
#include <libssh2_sftp.h>

#include "dataprovider.h"
#include "global.h"
#include "debug.h"
#include "transfer.h"
#include "urlparser.h"
#include "util.h"
#include "fileutil.h"
#include "stringutil.h"
#include "socket.h"

using std::fopen;
using std::fread;
using std::fclose;
using std::string;
using std::copy;
using std::strlen;

#define DEFAULT_MOUNTPOINT "/mnt"

//{{{ URLTransfer --------------------------------------------------------------

// -----------------------------------------------------------------------------
URLTransfer::URLTransfer(const char *url)
    throw (KError)
    : m_urlParser()
{
    m_urlParser.parseURL(url);
}

// -----------------------------------------------------------------------------
URLParser &URLTransfer::getURLParser()
    throw (KError)
{
    return m_urlParser;
}

//}}}
//{{{ FileTransfer -------------------------------------------------------------

FileTransfer::FileTransfer(const char *target_url)
    throw (KError)
    : URLTransfer(target_url), m_buffer(NULL)
{
    URLParser &parser = getURLParser();

    if (parser.getProtocol() != URLParser::PROT_FILE)
        throw KError("Only file URLs are allowed.");

    // create directory
    string dir = parser.getPath();
    FileUtil::mkdir(dir, true);

    // try to get the buffer size
    struct stat mystat;
    int err = stat(dir.c_str(), &mystat);
    if (err != 0)
        throw KSystemError("stat() on " + dir + " failed.", errno);

    m_bufferSize = mystat.st_size;
    if (m_bufferSize == 0)
        m_bufferSize = BUFSIZ;

    m_buffer = new char[m_bufferSize];
}

// -----------------------------------------------------------------------------
FileTransfer::~FileTransfer()
    throw ()
{
    delete[] m_buffer;
}

// -----------------------------------------------------------------------------
void FileTransfer::perform(DataProvider *dataprovider,
                           const char *target_file)
    throw (KError)
{
    FILE *fp = open(target_file);

    bool prepared = false;

    Debug::debug()->trace("FileTransfer::perform(%p, %s)",
        dataprovider, target_file);

    try {
        dataprovider->prepare();
        prepared = true;

        while (true) {
            size_t read_data = dataprovider->getData(m_buffer, m_bufferSize);

            // finished?
            if (read_data == 0)
                break;

            // sparse files
            if (read_data == m_bufferSize &&
                    Util::isZero(m_buffer, m_bufferSize)) {
                int ret = fseek(fp, m_bufferSize, SEEK_CUR);
                if (ret != 0)
                    throw KSystemError("FileTransfer::perform: fseek() failed.",
                        errno);
            } else {
                size_t ret = fwrite(m_buffer, 1, read_data, fp);
                if (ret != read_data)
                    throw KSystemError("FileTransfer::perform: fwrite() failed"
                        " with " + Stringutil::number2string(ret) +  ".", errno);
            }
        }
    } catch (...) {
        close(fp);
        if (prepared)
            dataprovider->finish();
        throw;
    }

    close(fp);
    dataprovider->finish();
}

// -----------------------------------------------------------------------------
FILE *FileTransfer::open(const char *target_file)
    throw (KError)
{
    Debug::debug()->trace("FileTransfer::open(%s)", target_file);

    string file = FileUtil::pathconcat(getURLParser().getPath(), target_file);

    FILE *fp = fopen(file.c_str(), "w");
    if (!fp)
        throw KSystemError("Error in fopen for " + file, errno);

    return fp;
}

// -----------------------------------------------------------------------------
void FileTransfer::close(FILE *fp)
    throw ()
{
    Debug::debug()->trace("FileTransfer::close()");

    fclose(fp);
}

//}}}
//{{{ FTPTransfer --------------------------------------------------------------

bool FTPTransfer::curl_global_inititalised = false;

// -----------------------------------------------------------------------------
static size_t curl_readfunction(void *buffer, size_t size, size_t nmemb,
                                void *data)
{
    DataProvider *dataprovider = reinterpret_cast<DataProvider *>(data);
    return dataprovider->getData((char *)buffer, size * nmemb);
}

// -----------------------------------------------------------------------------
static int curl_debug(CURL *curl, curl_infotype info, char *buffer,
                      size_t bufsiz,  void *data)
{
    (void)curl;
    (void)data;

    if (!Debug::debug()->isDebugEnabled())
        return 0;

    char *printbuffer = new char[bufsiz+1];
    copy(buffer, buffer+bufsiz, printbuffer);
    printbuffer[bufsiz] = 0;

    while (printbuffer[strlen(printbuffer)-1] == '\n')
        printbuffer[strlen(printbuffer)-1] = 0;

    switch (info) {
        case CURLINFO_TEXT:
            Debug::debug()->dbg("CURL: %s", printbuffer);
            break;

        case CURLINFO_HEADER_IN:
            Debug::debug()->trace("CURL: <- %s", printbuffer);
            break;

        case CURLINFO_HEADER_OUT:
            Debug::debug()->trace("CURL: %s", printbuffer);
            break;

        default:
            break;
    }

    delete[] printbuffer;

    return 0;
}

// -----------------------------------------------------------------------------
FTPTransfer::FTPTransfer(const char *target_url)
    throw (KError)
    : URLTransfer(target_url), m_curl(NULL)
{
    Debug::debug()->trace("FTPTransfer::FTPTransfer(%s)", target_url);

    // init the CURL library
    if (!curl_global_inititalised) {
        Debug::debug()->dbg("Calling curl_global_init()");
        CURLcode err = curl_global_init(CURL_GLOBAL_ALL);
        if (err != 0)
            throw KError("curl_global_init() failed.");
        curl_global_inititalised = true;
    }

    m_curl = curl_easy_init();
    if (!m_curl)
        throw KError("FTPTransfer::open(): curl_easy_init returned NULL");

    // error buffer
    CURLcode err = curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, m_curlError);
    if (err != CURLE_OK)
        throw KError("CURLOPT_ERRORBUFFER failed");

    // TODO: add configuration option
    err = curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, curl_debug);
    if (err != CURLE_OK)
        throw KError(string("CURL error: ") + m_curlError);

    err = curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, NULL);
    if (err != CURLE_OK)
        throw KError(string("CURL error: ") + m_curlError);

    err = curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1);
    if (err != CURLE_OK)
        throw KError(string("CURL error: ") + m_curlError);

    // read function
    err = curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, curl_readfunction);
    if (err != CURLE_OK)
        throw KError(string("CURL error: ") + m_curlError);

    // set upload
    err = curl_easy_setopt(m_curl, CURLOPT_UPLOAD, 1);
    if (err != CURLE_OK)
        throw KError(string("CURL error: ") + m_curlError);
}

// -----------------------------------------------------------------------------
FTPTransfer::~FTPTransfer()
    throw ()
{
    if (m_curl)
        curl_easy_cleanup(m_curl);
}

// -----------------------------------------------------------------------------
void FTPTransfer::perform(DataProvider *dataprovider,
                          const char *target_file)
    throw (KError)
{
    Debug::debug()->trace("FTPTransfer::perform(%p, %s)",
        dataprovider, target_file);

    open(dataprovider, target_file);

    try {
        dataprovider->prepare();

        CURLcode err = curl_easy_perform(m_curl);
        if (err != 0)
            throw KError(string("CURL error: ") + m_curlError);

        dataprovider->finish();
    } catch (...) {
        dataprovider->setError(true);
        dataprovider->finish();
        throw;
    }
}

// -----------------------------------------------------------------------------
void FTPTransfer::open(DataProvider *dataprovider,
                        const char *target_file)
    throw (KError)
{
    CURLcode err;

    Debug::debug()->trace("FTPTransfer::open(%p, %s)", dataprovider,
        target_file);

    // set the URL
    string full_url = FileUtil::pathconcat(getURLParser().getURL(), target_file);
    err = curl_easy_setopt(m_curl, CURLOPT_URL, full_url.c_str());
    if (err != CURLE_OK)
        throw KError(string("CURL error: ") + m_curlError);

    // read data
    err = curl_easy_setopt(m_curl, CURLOPT_READDATA, dataprovider);
    if (err != CURLE_OK)
        throw KError(string("CURL error: ") + m_curlError);
}

//}}}
//{{{ SFTPTransfer -------------------------------------------------------------

/* -------------------------------------------------------------------------- */
SFTPTransfer::SFTPTransfer(const char *target_url)
    throw (KError)
    : URLTransfer(target_url), m_sshSession(NULL), m_sftp(NULL), m_socket(NULL)
{
    Debug::debug()->trace("SFTPTransfer::SFTPTransfer(%s)", target_url);

    m_sshSession = libssh2_session_init();
    if (!m_sshSession)
        throw KError("libssh2_session_init() failed.");

    // set blocking
    libssh2_session_set_blocking(m_sshSession, 1);

    // create the socket and connect
    URLParser &parser = getURLParser();

    // get the correct port
    int port = parser.getPort();
    if (port <= 0)
        port = Socket::DP_SSH;

    m_socket = new Socket(parser.getHostname().c_str(), port, Socket::ST_TCP);
    int fd = m_socket->connect();

    // start it up
    int ret = libssh2_session_startup(m_sshSession, fd);
    if (ret != 0) {
        close();
        throw KError("libssh2_session_startup() failed with "+
            Stringutil::number2string(ret) +".");
    }

    // get the fingerprint for debugging
    // XXX: compare the fingerprint
    const char *fingerprint = libssh2_hostkey_hash(m_sshSession,
        LIBSSH2_HOSTKEY_HASH_MD5);
    Debug::debug()->info("SSH fingerprint: %s",
        Stringutil::bytes2hexstr(fingerprint, 16, true).c_str());

    string username = parser.getUsername();
    string password = parser.getPassword();

    ret = libssh2_userauth_password(m_sshSession, username.c_str(),
        password.c_str());
    if (ret != 0) {
        close();
        throw KError("libssh2_userauth_password() failed with "+
            Stringutil::number2string(ret) + ".");
    }

    // XXX: public key

    // SFTP session
    m_sftp = libssh2_sftp_init(m_sshSession);
    if (!m_sftp) {
        close();
        throw KError("libssh2_sftp_init() failed with "+
            Stringutil::number2string(ret) + ".");
    }

    mkdir(parser.getPath(), true);
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
void SFTPTransfer::mkdir(const string &dir, bool recursive)
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
void SFTPTransfer::perform(DataProvider *dataprovider, const char *target_file)
    throw (KError)
{
    Debug::debug()->trace("SFTPTransfer::perform(%p, %s)",
        dataprovider, target_file);

    bool prepared = false;


    LIBSSH2_SFTP_HANDLE  *handle = NULL;
    string file = FileUtil::pathconcat(getURLParser().getPath(), target_file);

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

//}}}
//{{{ NFSTransfer --------------------------------------------------------------

// -----------------------------------------------------------------------------
NFSTransfer::NFSTransfer(const char *target_url)
    throw (KError)
    : URLTransfer(target_url), m_mountpoint(""), m_fileTransfer(NULL)
{
    // mount the NFS share
    StringVector options;
    options.push_back("nolock");

    URLParser &parser = getURLParser();

    string mountedDir;
    FileUtil::nfsmount(parser.getHostname(), parser.getPath(),
        DEFAULT_MOUNTPOINT, options, mountedDir);


    m_mountpoint = DEFAULT_MOUNTPOINT;
    m_rest = parser.getPath();
    m_rest.replace(m_rest.begin(), m_rest.begin() + mountedDir.size(), "");
    m_rest = Stringutil::ltrim(m_rest, "/");

    m_prefix = FileUtil::pathconcat(m_mountpoint, m_rest);

    Debug::debug()->dbg("Mountpoint: %s, Rest: %s, Prefix: $s",
        m_mountpoint.c_str(), m_rest.c_str(), m_prefix.c_str());

    m_fileTransfer = new FileTransfer(("file://" + m_prefix).c_str());
}

// -----------------------------------------------------------------------------
NFSTransfer::~NFSTransfer()
    throw ()
{
    Debug::debug()->trace("NFSTransfer::~NFSTransfer()");

    try {
        close();
    } catch (const KError &kerror) {
        Debug::debug()->info("Error: %s", kerror.what());
    }
}

// -----------------------------------------------------------------------------
void NFSTransfer::perform(DataProvider *dataprovider,
                          const char *target_file)
    throw (KError)
{
    m_fileTransfer->perform(dataprovider, target_file);
}

// -----------------------------------------------------------------------------
void NFSTransfer::close()
    throw (KError)
{
    Debug::debug()->trace("NFSTransfer::close()");
    if (m_mountpoint.size() > 0) {
        FileUtil::umount(m_mountpoint);
        m_mountpoint.clear();
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

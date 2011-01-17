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
#if HAVE_LIBSSH2
#   include <libssh2.h>
#   include <libssh2_sftp.h>
#endif

#include "dataprovider.h"
#include "global.h"
#include "debug.h"
#include "transfer.h"
#include "urlparser.h"
#include "util.h"
#include "fileutil.h"
#include "stringutil.h"
#include "socket.h"
#include "configuration.h"

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

// -----------------------------------------------------------------------------
Transfer *URLTransfer::getTransfer(const char *url)
    throw (KError)
{
    Debug::debug()->trace("URLTransfer::getTransfer(%s)", url);

    URLParser parser;
    parser.parseURL(url);

    switch (parser.getProtocol()) {
        case URLParser::PROT_FILE:
            Debug::debug()->dbg("Returning FileTransfer");
            return new FileTransfer(url);

        case URLParser::PROT_FTP:
            Debug::debug()->dbg("Returning FTPTransfer");
            return new FTPTransfer(url);

#if HAVE_LIBSSH2
        case URLParser::PROT_SFTP:
            Debug::debug()->dbg("Returning SFTPTransfer");
            return new SFTPTransfer(url);
#endif // HAVE_LIBSSH2

        case URLParser::PROT_NFS:
            Debug::debug()->dbg("Returning NFSTransfer");
            return new NFSTransfer(url);

        case URLParser::PROT_CIFS:
            Debug::debug()->dbg("Returning CIFSTransfer");
            return new CIFSTransfer(url);

        default:
            throw KError("Unknown protocol.");
    }
}

//}}}
//{{{ FileTransfer -------------------------------------------------------------

// -----------------------------------------------------------------------------
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
    if (m_bufferSize == 0) {
        Debug::debug()->dbg("Buffer size of stat() is zero. Using %d.", BUFSIZ);
        m_bufferSize = BUFSIZ;
    }

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
                           const char *target_file,
                           bool *directSave)
    throw (KError)
{
    Debug::debug()->trace("FileTransfer::perform(%p, %s)",
        dataprovider, target_file);

    string fullTarget = FileUtil::pathconcat(
        getURLParser().getPath(), target_file);

    if (dataprovider->canSaveToFile()) {
        performFile(dataprovider, fullTarget.c_str());
        if (directSave)
            *directSave = true;
    } else {
        performPipe(dataprovider, fullTarget.c_str());
        if (directSave)
            *directSave = false;
    }
}

// -----------------------------------------------------------------------------
void FileTransfer::performFile(DataProvider *dataprovider,
                               const char *target_file)
    throw (KError)
{
    Debug::debug()->trace("FileTransfer::performFile(%p, %s)",
        dataprovider, target_file);

    dataprovider->saveToFile(target_file);
}

// -----------------------------------------------------------------------------
void FileTransfer::performPipe(DataProvider *dataprovider,
                               const char *target_file)
    throw (KError)
{
    Debug::debug()->trace("FileTransfer::performPipe(%p, %s)",
        dataprovider, target_file);

    FILE *fp = open(target_file);
    bool sparse = !Configuration::config()->kdumptoolContainsFlag("NOSPARSE");
    if (!sparse)
        Debug::debug()->info("Creation of sparse files disabled in "
            "configuration.");

    bool prepared = false;

    bool last_was_sparse = false;
    try {
        dataprovider->prepare();
        prepared = true;

        while (true) {
            size_t read_data = dataprovider->getData(m_buffer, m_bufferSize);

            // finished?
            if (read_data == 0)
                break;

            // sparse files
            if (sparse && read_data == m_bufferSize &&
                    Util::isZero(m_buffer, m_bufferSize)) {
                int ret = fseek(fp, m_bufferSize, SEEK_CUR);
                if (ret != 0)
                    throw KSystemError("FileTransfer::perform: fseek() failed.",
                        errno);
                last_was_sparse = true;
            } else {
                size_t ret = fwrite(m_buffer, 1, read_data, fp);
                if (ret != read_data)
                    throw KSystemError("FileTransfer::perform: fwrite() failed"
                        " with " + Stringutil::number2string(ret) +  ".", errno);
                last_was_sparse = false;
            }
        }

        if (last_was_sparse) {

            loff_t old_offset = ftell(fp);

            // write something
            int ret = fputc('\0', fp);
            if (ret == EOF)
                throw KSystemError("Unable to write.", errno);

            // truncate the file
            rewind(fp);
            ret = ftruncate(fileno(fp), old_offset);
            if (ret != 0)
                throw KSystemError("Unable to set the file position.", errno);
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

    FILE *fp = fopen(target_file, "w");
    if (!fp)
        throw KSystemError("Error in fopen for " + string(target_file), errno);

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

    // create directory
    err = curl_easy_setopt(m_curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 1);
    if (err != CURLE_OK)
        throw KError(string("CURL error: " ) + m_curlError);

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
                          const char *target_file,
                          bool *directSave)
    throw (KError)
{
    Debug::debug()->trace("FTPTransfer::perform(%p, %s)",
        dataprovider, target_file);

    if (directSave)
        *directSave = false;
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

#if HAVE_LIBSSH2

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

    // username and password
    string username = parser.getUsername();
    string password = parser.getPassword();

    // public and private key
    string homedir = getenv("HOME");
    string pubkey = FileUtil::pathconcat(homedir, ".ssh", "id_dsa.pub");
    string privkey = FileUtil::pathconcat(homedir, ".ssh", "id_dsa");

    if (FileUtil::exists(pubkey) && FileUtil::exists(privkey)) {
        Debug::debug()->dbg("Using private key %s and public key %s",
                privkey.c_str(), pubkey.c_str());

        ret = libssh2_userauth_publickey_fromfile(m_sshSession,
                username.c_str(), pubkey.c_str(), privkey.c_str(),
                password.c_str());
        if (ret != 0) {
            close();
            throw KError("libssh2_userauth_password() failed with "+
                Stringutil::number2string(ret) + ".");
        }
    } else {
        Debug::debug()->dbg("Using password auth");

        ret = libssh2_userauth_password(m_sshSession, username.c_str(),
            password.c_str());
        if (ret != 0) {
            close();
            throw KError("libssh2_userauth_password() failed with "+
                Stringutil::number2string(ret) + ".");
        }
    }

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
void SFTPTransfer::perform(DataProvider *dataprovider,
                           const char *target_file,
                           bool *directSave)
    throw (KError)
{
    Debug::debug()->trace("SFTPTransfer::perform(%p, %s)",
        dataprovider, target_file);

    bool prepared = false;
    if (directSave)
        *directSave = false;

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

#endif // HAVE_LIBSSH2

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

    string mountedDir = FileUtil::dirname(parser.getPath());
    FileUtil::nfsmount(parser.getHostname(), mountedDir,
        DEFAULT_MOUNTPOINT, options);


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
                          const char *target_file,
                          bool *directSave)
    throw (KError)
{
    m_fileTransfer->perform(dataprovider, target_file, directSave);
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
//{{{ CIFSTransfer -------------------------------------------------------------

// -----------------------------------------------------------------------------
CIFSTransfer::CIFSTransfer(const char *target_url)
    throw (KError)
    : URLTransfer(target_url), m_mountpoint(""), m_fileTransfer(NULL)
{
    URLParser &parser = getURLParser();

    string share = parser.getPath();
    share = Stringutil::ltrim(share, "/");
    string::size_type first_slash = share.find("/");
    if (first_slash == string::npos)
        throw KError("Path ("+ parser.getPath() +") must contain a \"/\".");
    share = share.substr(0, first_slash);

    StringVector options;
    if (parser.getUsername().size() > 0) {
        options.push_back("user=" + parser.getUsername());
        if (parser.getPassword().size() > 0)
            options.push_back("password=" + parser.getPassword());
    }
    if (parser.getPort() != -1) {
        options.push_back("port=" + Stringutil::number2string(parser.getPort()));
    }

    FileUtil::mount("//" + parser.getHostname() + "/" + share,
        DEFAULT_MOUNTPOINT, "cifs", options);
    m_mountpoint = DEFAULT_MOUNTPOINT;

    string rest = parser.getPath();
    string::size_type shareBegin = rest.find(share);
    if (shareBegin == string::npos) {
        close();
        throw KError("Internal error in CIFSTransfer::CIFSTransfer.");
    }
    rest = rest.substr(shareBegin + share.size());
    string prefix = FileUtil::pathconcat(m_mountpoint, rest);

    Debug::debug()->dbg("Mountpoint: %s, Rest: %s, Prefix: %s",
        m_mountpoint.c_str(), rest.c_str(), prefix.c_str());

    m_fileTransfer = new FileTransfer(("file://" + prefix).c_str());
}

// -----------------------------------------------------------------------------
CIFSTransfer::~CIFSTransfer()
    throw ()
{
    Debug::debug()->trace("CIFSTransfer::~CIFSTransfer()");

    try {
        close();
    } catch (const KError &kerror) {
        Debug::debug()->info("Error: %s", kerror.what());
    }
}

// -----------------------------------------------------------------------------
void CIFSTransfer::perform(DataProvider *dataprovider,
                          const char *target_file,
                          bool *directSave)
    throw (KError)
{
    m_fileTransfer->perform(dataprovider, target_file, directSave);
}

// -----------------------------------------------------------------------------
void CIFSTransfer::close()
    throw (KError)
{
    Debug::debug()->trace("CIFSTransfer::close()");
    if (m_mountpoint.size() > 0) {
        FileUtil::umount(m_mountpoint);
        m_mountpoint.clear();
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

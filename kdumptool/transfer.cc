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
#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <cerrno>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

#include <curl/curl.h>

#include "dataprovider.h"
#include "global.h"
#include "debug.h"
#include "transfer.h"
#include "rootdirurl.h"
#include "util.h"
#include "fileutil.h"
#include "stringutil.h"
#include "configuration.h"
#include "sshtransfer.h"

using std::fopen;
using std::fread;
using std::fclose;
using std::string;
using std::copy;
using std::strlen;
using std::cerr;
using std::endl;

#define DEFAULT_MOUNTPOINT "/mnt"

//{{{ Transfer -----------------------------------------------------------------

// -----------------------------------------------------------------------------
void Transfer::perform(DataProvider *dataprovider,
		       const std::string &target_file,
		       bool *directSave)
    throw (KError)
{
    const StringVector target_files(1, target_file);
    perform(dataprovider, target_files, directSave);
}

//{{{ URLTransfer --------------------------------------------------------------

// -----------------------------------------------------------------------------
URLTransfer::URLTransfer(const RootDirURLVector &urlv, const string &subdir)
    throw (KError)
    : m_urlVector(urlv), m_subDir(subdir)
{
}

// -----------------------------------------------------------------------------
Transfer *URLTransfer::getTransfer(const RootDirURLVector &urlv,
				   const string &subdir)
    throw (KError)
{
    Debug::debug()->trace("URLTransfer::getTransfer(%p, \"%s\")",
			  &urlv, subdir.c_str());

    if (urlv.size() == 0)
	throw KError("No target specified!");

    switch (urlv.begin()->getProtocol()) {
        case URLParser::PROT_FILE:
            Debug::debug()->dbg("Returning FileTransfer");
            return new FileTransfer(urlv, subdir);

        case URLParser::PROT_FTP:
            Debug::debug()->dbg("Returning FTPTransfer");
            return new FTPTransfer(urlv, subdir);

#if HAVE_LIBSSH2
        case URLParser::PROT_SFTP:
            Debug::debug()->dbg("Returning SFTPTransfer");
            return new SFTPTransfer(urlv, subdir);
#endif // HAVE_LIBSSH2

        case URLParser::PROT_SSH:
	    Debug::debug()->dbg("Returning SSHTransfer");
	    return new SSHTransfer(urlv, subdir);

        case URLParser::PROT_NFS:
            Debug::debug()->dbg("Returning NFSTransfer");
            return new NFSTransfer(urlv, subdir);

        case URLParser::PROT_CIFS:
            Debug::debug()->dbg("Returning CIFSTransfer");
            return new CIFSTransfer(urlv, subdir);

        default:
            throw KError("Unknown protocol.");
    }
}

//}}}
//{{{ FileTransfer -------------------------------------------------------------

// -----------------------------------------------------------------------------
FileTransfer::FileTransfer(const RootDirURLVector &urlv,
			   const std::string &subdir)
    throw (KError)
    : URLTransfer(urlv, subdir), m_bufferSize(0), m_buffer(NULL)
{
    RootDirURLVector::const_iterator it;
    for (it = urlv.begin(); it != urlv.end(); ++it)
	if (it->getProtocol() != URLParser::PROT_FILE)
	    throw KError("Only file URLs are allowed for split.");

    // create directories
    for (it = urlv.begin(); it != urlv.end(); ++it) {
        FilePath dir = it->getRealPath();
        dir.appendPath(subdir);
        dir.mkdir(true);
    }

    // try to get the buffer size
    for (it = urlv.begin(); it != urlv.end(); ++it) {
	struct stat mystat;
	int err = stat(it->getRealPath().c_str(), &mystat);
	if (err == 0 && (size_t)mystat.st_blksize > m_bufferSize)
	    m_bufferSize = mystat.st_blksize;
    }

    if (m_bufferSize == 0) {
        Debug::debug()->dbg("Cannot determine block size. Using %d.", BUFSIZ);
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
                           const StringVector &target_files,
                           bool *directSave)
    throw (KError)
{
    Debug::debug()->trace("FileTransfer::perform(%p, [ \"%s\"%s ])",
	dataprovider, target_files.front().c_str(),
	target_files.size() > 1 ? ", ..." : "");

    StringVector full_targets;
    StringVector::const_iterator it;
    RootDirURLVector &urlv = getURLVector();
    RootDirURLVector::const_iterator itv = urlv.begin();
    for (it = target_files.begin(); it != target_files.end(); ++it) {
        FilePath fp = itv->getRealPath();
        full_targets.push_back(fp.appendPath(getSubDir()).appendPath(*it));
	if (++itv == urlv.end())
	    itv = urlv.begin();
    }

    if (dataprovider->canSaveToFile()) {
	performFile(dataprovider, full_targets);
        if (directSave)
            *directSave = true;
    } else {
        performPipe(dataprovider, full_targets);
        if (directSave)
            *directSave = false;
    }
}

// -----------------------------------------------------------------------------
void FileTransfer::performFile(DataProvider *dataprovider,
			       const StringVector &target_files)
    throw (KError)
{
    Debug::debug()->trace("FileTransfer::performFile(%p, [ \"%s\"%s ])",
	dataprovider, target_files.front().c_str(),
	target_files.size() > 1 ? ", ..." : "");

    dataprovider->saveToFile(target_files);
}

// -----------------------------------------------------------------------------
void FileTransfer::performPipe(DataProvider *dataprovider,
			       const StringVector &target_files)
    throw (KError)
{
    Debug::debug()->trace("FileTransfer::performPipe(%p, [ \"%s\"%s ])",
        dataprovider, target_files.front().c_str(),
	target_files.size() > 1 ? ", ..." : "");

    if (target_files.size() > 1)
	cerr << "WARNING: First dump target used; rest ignored." << endl;

    FILE *fp = open(target_files.front().c_str());
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
FILE *FileTransfer::open(const string &target_file)
    throw (KError)
{
    Debug::debug()->trace("FileTransfer::open(%s)", target_file.c_str());

    FILE *fp = fopen(target_file.c_str(), "w");
    if (!fp)
        throw KSystemError("Error in fopen for " + target_file, errno);

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
FTPTransfer::FTPTransfer(const RootDirURLVector &urlv,
			 const std::string &subdir)
    throw (KError)
    : URLTransfer(urlv, subdir), m_curl(NULL)
{
    if (urlv.size() > 1)
	cerr << "WARNING: First dump target used; rest ignored." << endl;
    const RootDirURL &parser = urlv.front();

    Debug::debug()->trace("FTPTransfer::FTPTransfer(%s)",
			  parser.getURL().c_str());

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
                          const StringVector &target_files,
                          bool *directSave)
    throw (KError)
{
    Debug::debug()->trace("FTPTransfer::perform(%p, [ \"%s\"%s ])",
	dataprovider, target_files.front().c_str(),
	target_files.size() > 1 ? ", ..." : "");

    if (directSave)
        *directSave = false;
    open(dataprovider, target_files.front().c_str());

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
                        const string &target_file)
    throw (KError)
{
    CURLcode err;

    Debug::debug()->trace("FTPTransfer::open(%p, %s)", dataprovider,
        target_file.c_str());

    RootDirURLVector &urlv = getURLVector();
    const RootDirURL &parser = urlv.front();

    // set the URL
    FilePath full_url = parser.getURL();
    full_url.appendPath(getSubDir()).appendPath(target_file);
    err = curl_easy_setopt(m_curl, CURLOPT_URL, full_url.c_str());
    if (err != CURLE_OK)
        throw KError(string("CURL error: ") + m_curlError);

    // read data
    err = curl_easy_setopt(m_curl, CURLOPT_READDATA, dataprovider);
    if (err != CURLE_OK)
        throw KError(string("CURL error: ") + m_curlError);
}

//}}}
//{{{ NFSTransfer --------------------------------------------------------------

// -----------------------------------------------------------------------------
NFSTransfer::NFSTransfer(const RootDirURLVector &urlv,
			 const std::string &subdir)
    throw (KError)
    : URLTransfer(urlv, subdir), m_mountpoint(""), m_fileTransfer(NULL)
{
    RootDirURLVector file_urlv("", "");
    RootDirURLVector::const_iterator it;
    for (it = urlv.begin(); it != urlv.end(); ++it)
	file_urlv.push_back(translate(*it));
    m_fileTransfer = new FileTransfer(file_urlv, subdir);
}

// -----------------------------------------------------------------------------
RootDirURL NFSTransfer::translate(const RootDirURL &parser)
    throw (KError)
{
    // mount the NFS share
    StringVector options;
    options.push_back("nolock");

    string mountedDir = parser.getPath();
    FileUtil::nfsmount(parser.getHostname(), mountedDir,
        DEFAULT_MOUNTPOINT, options);


    m_mountpoint = DEFAULT_MOUNTPOINT;
    m_rest = parser.getPath();
    m_rest.replace(m_rest.begin(), m_rest.begin() + mountedDir.size(), "");
    m_rest.ltrim("/");

    (m_prefix = m_mountpoint).appendPath(m_rest);

    Debug::debug()->dbg("Mountpoint: %s, Rest: %s, Prefix: $s",
        m_mountpoint.c_str(), m_rest.c_str(), m_prefix.c_str());

    return RootDirURL("file://" + m_prefix, "");
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
                          const StringVector &target_files,
                          bool *directSave)
    throw (KError)
{
    m_fileTransfer->perform(dataprovider, target_files, directSave);
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
CIFSTransfer::CIFSTransfer(const RootDirURLVector &urlv,
			   const std::string &subdir)
    throw (KError)
    : URLTransfer(urlv, subdir), m_mountpoint(""), m_fileTransfer(NULL)
{
    RootDirURLVector file_urlv("", "");
    RootDirURLVector::const_iterator it;
    for (it = urlv.begin(); it != urlv.end(); ++it)
	file_urlv.push_back(translate(*it));
    m_fileTransfer = new FileTransfer(file_urlv, subdir);
}

// -----------------------------------------------------------------------------
RootDirURL CIFSTransfer::translate(const RootDirURL &parser)
    throw (KError)
{
    KString share = parser.getPath();
    share.ltrim("/");
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
    FilePath prefix = m_mountpoint;
    prefix.appendPath(rest);

    Debug::debug()->dbg("Mountpoint: %s, Rest: %s, Prefix: %s",
        m_mountpoint.c_str(), rest.c_str(), prefix.c_str());

    return RootDirURL("file://" + prefix, "");
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
                          const StringVector &target_files,
                          bool *directSave)
    throw (KError)
{
    m_fileTransfer->perform(dataprovider, target_files, directSave);
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

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
#ifndef TRANSFER_H
#define TRANSFER_H

#include <cstdio>
#include <cstdarg>

#include <curl/curl.h>
#include <libssh2.h>
#include <libssh2_sftp.h>

#include "global.h"
#include "urlparser.h"
#include "socket.h"

class DataProvider;

//{{{ Transfer -----------------------------------------------------------------

/**
 * Interface that provides a capability to transfer data between A and B.
 *
 * Some methods are interface-specific.
 */
class Transfer {

    public:

        /**
         * Destructor.
         */
        virtual ~Transfer()
        throw () {}

        /**
         * Performs the actual transfer. This means that
         *
         *  - at first, DataProvider::prepare() is called,
         *  - then repeadedly DataProvider::getData() is called,
         *  - at last, DataProvider::finish() is called.
         *
         * It's perfectly valid to call Transfer::perform() on multiple
         * DataProvider objects.
         *
         * @param[in] dataprovider the data provider
         * @param[in] target_url the URL for the target directory, i.e. without
         *            the file name
         * @param[in] target_file the actual file name for the target
         * @param[out] directSave if the transfer used
         *             DataProvider::saveToFile()
         * @exception KError on any error
         */
        virtual void perform(DataProvider *dataprovider,
                             const char *target_file,
                             bool *directSave)
        throw (KError) = 0;

};

//}}}
//{{{ URLTransfer --------------------------------------------------------------

/**
 * Base class for all Transfer implementations that unifies URL handling.
 */
class URLTransfer : public Transfer {

    public:

        /**
         * Destructor.
         */
        virtual ~URLTransfer()
        throw () {}

        /**
         * Creates a new URLTransfer object.
         *
         * @param[in] url the URL
         * @throw KError if parsing the URL failed
         */
        URLTransfer(const char *url)
        throw (KError);

        /**
         * Returns the URL parser.
         *
         * @return reference to the URL parser.
         */
        URLParser &getURLParser()
        throw (KError);

        /**
         * Returns a Transfer object suitable to the provided URL.
         *
         * @param[in] url the URL
         * @return the Transfer object
         *
         * @exception KError if parsing the URL failed or there's no
         *            implementation for that class.
         */
        static Transfer *getTransfer(const char *url)
        throw (KError);

    private:
        URLParser m_urlParser;
};

//}}}
//{{{ FileTransfer -------------------------------------------------------------

/**
 * Transfers files.
 */
class FileTransfer : public URLTransfer {

    public:

        /**
         * Creates a new FileTransfer object.
         *
         * @param[in] target_url the directory
         * @throw KError if parsing the URL or creating the directory failed
         */
        FileTransfer(const char *target_url)
        throw (KError);

        /**
         * Destroys a FileTransfer object.
         */
        ~FileTransfer()
        throw ();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const char *target_file,
                     bool *directSave)
        throw (KError);

    protected:

        void performFile(DataProvider *dataprovider,
                         const char *target_file)
        throw (KError);

        void performPipe(DataProvider *dataprovider,
                         const char *target_file)
        throw (KError);

        FILE *open(const char *target_file)
        throw (KError);

        void close(FILE *fp)
        throw ();

    private:
        size_t m_bufferSize;
        char *m_buffer;
};

//}}}
//{{{ FTPTransfer --------------------------------------------------------------

/**
 * Transfers a file to FTP (upload).
 */
class FTPTransfer : public URLTransfer {

    public:

        /**
         * Creates a global FTPTransfer object.
         *
         * @exception KError when initialising the underlying library fails
         */
        FTPTransfer(const char *target_url)
        throw (KError);

        /**
         * Destroys a FTPTransfer object.
         */
        ~FTPTransfer()
        throw ();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const char *target_file,
                     bool *directSave)
        throw (KError);

    protected:

        void open(DataProvider *dataprovider,
                   const char *target_file)
        throw (KError);

    private:
        char m_curlError[CURL_ERROR_SIZE];
        static bool curl_global_inititalised;
        CURL *m_curl;
};

//}}}
//{{{ SFTPTransfer -------------------------------------------------------------

/**
 * Transfers a file to SFTP (upload).
 */
class SFTPTransfer : public URLTransfer {

    public:

        /**
         * Creates a SFTPTransfer object.
         *
         * @exception KError when initialising the underlying library fails
         */
        SFTPTransfer(const char *target_url)
        throw (KError);

        /**
         * Destroys a SFTPTransfer object.
         */
        ~SFTPTransfer()
        throw ();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const char *target_file,
                     bool *directSave)
        throw (KError);

    protected:
        void close()
        throw ();

        void mkdir(const std::string &dir, bool recursive)
        throw (KError);

        bool exists(const std::string &file)
        throw (KError);

    private:
        LIBSSH2_SESSION *m_sshSession;
        LIBSSH2_SFTP *m_sftp;
        Socket *m_socket;
        char m_buffer[BUFSIZ];

};

//}}}
//{{{ NFSTransfer -------------------------------------------------------------

/**
 * Transfers a file over NFS.
 */
class NFSTransfer : public URLTransfer {

    public:

        /**
         * Creates a NFSTransfer object.
         *
         * @exception KError when mounting the share failes
         */
        NFSTransfer(const char *target_url)
        throw (KError);

        /**
         * Destroys a SFTPTransfer object.
         */
        ~NFSTransfer()
        throw ();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const char *target_file,
                     bool *directSave)
        throw (KError);

    protected:
        void close()
        throw (KError);

    private:
        std::string m_mountpoint;
        std::string m_rest;
        std::string m_prefix;
        FileTransfer *m_fileTransfer;
};

//}}}
//{{{ CIFSTransfer -------------------------------------------------------------

/**
 * Transfers a file over CIFS (SMB).
 */
class CIFSTransfer : public URLTransfer {

    public:

        /**
         * Creates a CIFSTransfer object.
         *
         * @exception KError when mounting the share failes
         */
        CIFSTransfer(const char *target_url)
        throw (KError);

        /**
         * Destroys a CIFSTransfer object.
         */
        ~CIFSTransfer()
        throw ();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const char *target_file,
                     bool *directSave)
        throw (KError);

    protected:
        void close()
        throw (KError);

    private:
        std::string m_mountpoint;
        FileTransfer *m_fileTransfer;
};

//}}}

#endif /* TRANSFER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

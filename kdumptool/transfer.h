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

#include "global.h"
#include "stringutil.h"
#include "fileutil.h"
#include "rootdirurl.h"
#include "stringvector.h"

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
        { }

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
         * @param[in] target_file the actual file name for the target
         * @param[out] directSave if the transfer used
         *             DataProvider::saveToFile()
         * @exception KError on any error
         */
        virtual void perform(DataProvider *dataprovider,
                             const StringVector &target_files,
                             bool *directSave=NULL) = 0;

	/**
	 * Shorthand version of perform() if there is only one file
	 *
	 * @param[in] dataprovider the data provider
	 * @param[in] target_file the file name for the target
	 * @param[out] directSave @see Transfer::perform()
         * @exception KError on any error
	 */
	void perform(DataProvider *dataprovider,
		     const std::string &target_file,
		     bool *directSave=NULL);
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
        { }

        /**
         * Creates a new URLTransfer object.
         *
         * @param[in] urlv target URLs
         * @throw KError if parsing the URL failed
         */
        URLTransfer(const RootDirURLVector &urlv);

        /**
         * Returns the URL parser.
         *
         * @return reference to the URL vector.
         */
        RootDirURLVector &getURLVector()
	{ return m_urlVector; }

    private:
        RootDirURLVector m_urlVector;
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
         * @param[in] urlv target directories
         * @throw KError if parsing the URL or creating the directory failed
         */
        FileTransfer(const RootDirURLVector &urlv);

        /**
         * Destroys a FileTransfer object.
         */
        ~FileTransfer();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const StringVector &target_files,
                     bool *directSave);

    protected:

        void performFile(DataProvider *dataprovider,
			 const StringVector &target_files);

        void performPipe(DataProvider *dataprovider,
			 const StringVector &target_files);

        FILE *open(const std::string &target_file);

        void close(FILE *fp);

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
        FTPTransfer(const RootDirURLVector &urlv);

        /**
         * Destroys a FTPTransfer object.
         */
        ~FTPTransfer();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const StringVector &target_files,
                     bool *directSave);

    protected:

        void open(DataProvider *dataprovider,
		  const std::string &target_file);

    private:
        char m_curlError[CURL_ERROR_SIZE];
        static bool curl_global_inititalised;
        CURL *m_curl;
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
        NFSTransfer(const RootDirURLVector &urlv);

        /**
         * Destroys a SFTPTransfer object.
         */
        ~NFSTransfer();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const StringVector &target_files,
                     bool *directSave);

    protected:
        void close();

        /**
	 * Translate a NFS URL into a file URL
	 *
	 * @return RootDirurl
	 */
	RootDirURL translate(const RootDirURL &url);


    private:
        FilePath m_mountpoint;
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
        CIFSTransfer(const RootDirURLVector &urlv);

        /**
         * Destroys a CIFSTransfer object.
         */
        ~CIFSTransfer();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const StringVector &target_files,
                     bool *directSave);

    protected:
        void close();

        /**
	 * Translate a CIFS URL into a file URL
	 *
	 * @return RootDirurl
	 */
	RootDirURL translate(const RootDirURL &url);

    private:
        std::string m_mountpoint;
        FileTransfer *m_fileTransfer;
};

//}}}

#endif /* TRANSFER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

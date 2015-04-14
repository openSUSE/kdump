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
#ifndef SSHTRANSFER_H
#define SSHTRANSFER_H

#include "global.h"
#include "stringutil.h"
#include "fileutil.h"
#include "rootdirurl.h"
#include "process.h"
#include "socket.h"
#include "transfer.h"

//{{{ SSHTransfer --------------------------------------------------------------

/**
 * Transfers a file to SSH (upload).
 */
class SSHTransfer : public URLTransfer {

    public:

        /**
         * Creates a SSHTransfer object.
         *
         * @exception KError when initialising the underlying library fails
         */
        SSHTransfer(const RootDirURLVector &urlv, const std::string &subdir)
        throw (KError);

        /**
         * Destroys a SSHTransfer object.
         */
        ~SSHTransfer()
        throw ();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const StringVector &target_files,
                     bool *directSave)
        throw (KError);

    private:
        char m_buffer[BUFSIZ];

	StringVector makeArgs(std::string const &remote);
};

//}}}
//{{{ SFTPPacket ---------------------------------------------------------------

/**
 * Encode/decode an SFTP packet.
 */
class SFTPPacket {

    public:

	SFTPPacket(void);

	ByteVector const &data(void) const
	throw ()
	{ return m_vector; }

	ByteVector const &update(void);

	void setData(ByteVector const &val)
	{
	    m_vector = val;
	    m_gpos = 0;
	}

	void addByte(unsigned char val)
	{ m_vector.push_back(val); }

	void addByteVector(ByteVector const &val);

	void addInt32(unsigned long val);

	void addInt64(unsigned long long val);

	void addString(KString const &val);

	unsigned char getByte(void)
	{ return m_vector.at(m_gpos++); }

	unsigned long getInt32(void);

	unsigned long long getInt64(void);

	std::string getString(void);

    private:
	ByteVector m_vector;
	size_t m_gpos;
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
        SFTPTransfer(const RootDirURLVector &urlv, const std::string &subdir)
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
                     const StringVector &target_files,
                     bool *directSave)
        throw (KError);

    private:
	SubProcess m_process;
	int m_fdreq, m_fdresp;

	StringVector makeArgs(void);
};

//}}}

#endif /* SSHTRANSFER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

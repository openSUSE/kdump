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
        SSHTransfer(const RootDirURLVector &urlv);

        /**
         * Destroys a SSHTransfer object.
         */
        ~SSHTransfer();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const StringVector &target_files,
                     bool *directSave);

    private:
        char m_buffer[BUFSIZ];

	StringVector makeArgs(std::string const &remote);
};

//}}}
//{{{ KSFTPErrorCode -----------------------------------------------------------

/**
 * Error codes defined by the protocol
 */
enum {
    SSH_FX_OK			  =  0,
    SSH_FX_EOF			  =  1,
    SSH_FX_NO_SUCH_FILE		  =  2,
    SSH_FX_PERMISSION_DENIED	  =  3,
    SSH_FX_FAILURE		  =  4,
    SSH_FX_BAD_MESSAGE		  =  5,
    SSH_FX_NO_CONNECTION	  =  6,
    SSH_FX_CONNECTION_LOST	  =  7,
    SSH_FX_OP_UNSUPPORTED	  =  8,
    SSH_FX_INVALID_HANDLE	  =  9,
    SSH_FX_NO_SUCH_PATH		  = 10,
    SSH_FX_FILE_ALREADY_EXISTS	  = 11,
    SSH_FX_WRITE_PROTECT	  = 12,
    SSH_FX_NO_MEDIA		  = 13,
    SSH_FX_NO_SPACE_ON_FILESYSTEM = 14,
    SSH_FX_QUOTA_EXCEEDED	  = 15,
    SSH_FX_UNKNOWN_PRINCIPAL	  = 16,
    SSH_FX_LOCK_CONFLICT	  = 17,
    SSH_FX_DIR_NOT_EMPTY	  = 18,
    SSH_FX_NOT_A_DIRECTORY	  = 19,
    SSH_FX_INVALID_FILENAME	  = 20,
    SSH_FX_LINK_LOOP		  = 21,
};

/**
 * Class for sftp errors.
 */
class KSFTPErrorCode : public KErrorCode {

    public:
        KSFTPErrorCode(int code)
            : KErrorCode(code)
        {}

        virtual std::string message(void) const;
};

typedef KCodeError<KSFTPErrorCode> KSFTPError;

//}}}
//{{{ SFTPPacket ---------------------------------------------------------------

/**
 * Packet types defined by protocol (incomplete list)
 */
enum {
    SSH_FXP_INIT	=   1,
    SSH_FXP_VERSION	=   2,
    SSH_FXP_OPEN	=   3,
    SSH_FXP_CLOSE	=   4,
    SSH_FXP_WRITE	=   6,
    SSH_FXP_MKDIR	=  14,
    SSH_FXP_STAT	=  17,
    SSH_FXP_STATUS	= 101,
    SSH_FXP_HANDLE	= 102,
    SSH_FXP_ATTRS	= 105,
};

/**
 * Open flags
 */
enum {
    SSH_FXF_READ	= 0x00000001,
    SSH_FXF_WRITE	= 0x00000002,
    SSH_FXF_APPEND	= 0x00000004,
    SSH_FXF_CREAT	= 0x00000008,
    SSH_FXF_TRUNC	= 0x00000010,
    SSH_FXF_EXCL	= 0x00000020,
};

/**
 * Encode/decode an SFTP packet.
 */
class SFTPPacket {

    public:

	SFTPPacket(void);

	ByteVector const &data(void) const
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
        SFTPTransfer(const RootDirURLVector &urlv);

        /**
         * Destroys a SFTPTransfer object.
         */
        ~SFTPTransfer();

        /**
         * Transfers the file.
         *
         * @see Transfer::perform()
         */
        void perform(DataProvider *dataprovider,
                     const StringVector &target_files,
                     bool *directSave);

    protected:
	static const int MY_PROTO_VER = 3; // our advertised version

        bool exists(const std::string &file);
        void mkpath(const std::string &path);
	std::string createfile(const std::string &file);
	void closefile(const std::string &handle);
	void writefile(const std::string &handle, off_t off,
		       const ByteVector &data);

    private:
	SubProcess m_process;
	int m_fdreq, m_fdresp;
	unsigned long m_proto_ver; // remote SFTP protocol version
	unsigned long m_lastid;

	StringVector makeArgs(void);

	unsigned long nextId(void)
	{ return m_lastid = (m_lastid + 1) & ((1UL << 32) - 1); }

	void sendPacket(SFTPPacket &pkt);
	void recvPacket(SFTPPacket &pkt);
	void recvBuffer(unsigned char *bufp, size_t buflen);
};

//}}}

#endif /* SSHTRANSFER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

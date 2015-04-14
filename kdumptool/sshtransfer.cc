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
#include <unistd.h>

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
    : m_vector(sizeof(uint32_t)),
      m_gpos(0)
{
}

/* -------------------------------------------------------------------------- */
void SFTPPacket::addByteVector(ByteVector const &val)
{
    m_vector.insert(m_vector.end(), val.begin(), val.end());
}

/* -------------------------------------------------------------------------- */
void SFTPPacket::addInt32(unsigned long val)
{
    int i;
    for (i = sizeof(uint32_t) - 1; i >= 0; --i)
	m_vector.push_back((val >> (i*8)) & 0xff);
}

/* -------------------------------------------------------------------------- */
unsigned long SFTPPacket::getInt32(void)
{
    size_t i;
    unsigned long ret = 0UL;
    for (i = 0; i < sizeof(uint32_t); ++i) {
	ret <<= 8;
	ret |= m_vector.at(m_gpos++);
    }
    return ret;
}

/* -------------------------------------------------------------------------- */
void SFTPPacket::addInt64(unsigned long long val)
{
    int i;
    for (i = sizeof(uint64_t) - 1; i >= 0; --i)
	m_vector.push_back((val >> (i*8)) & 0xff);
}

/* -------------------------------------------------------------------------- */
unsigned long long SFTPPacket::getInt64(void)
{
    size_t i;
    unsigned long long ret = 0ULL;
    for (i = 0; i < sizeof(uint64_t); ++i) {
	ret <<= 8;
	ret |= m_vector.at(m_gpos++);
    }
    return ret;
}

/* -------------------------------------------------------------------------- */
void SFTPPacket::addString(KString const &val)
{
    addInt32(val.length());
    m_vector.insert(m_vector.end(), val.begin(), val.end());
}

/* -------------------------------------------------------------------------- */
std::string SFTPPacket::getString(void)
{
    unsigned long len = getInt32();
    ByteVector::iterator it = m_vector.begin() + m_gpos;
    return string(it, it + len);
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

/* -------------------------------------------------------------------------- */
SFTPTransfer::SFTPTransfer(const RootDirURLVector &urlv,
			   const std::string &subdir)
    throw (KError)
    : URLTransfer(urlv, subdir)
{
    if (urlv.size() > 1)
	cerr << "WARNING: First dump target used; rest ignored." << endl;
    const RootDirURL &parser = urlv.front();

    Debug::debug()->trace("SFTPTransfer::SFTPTransfer(%s)",
			  parser.getURL().c_str());

    m_process.setPipeDirection(STDIN_FILENO, SubProcess::ParentToChild);
    m_process.setPipeDirection(STDOUT_FILENO, SubProcess::ChildToParent);
    m_process.spawn("ssh", makeArgs());

    m_fdreq = m_process.getPipeFD(STDIN_FILENO);
    m_fdresp = m_process.getPipeFD(STDOUT_FILENO);
}

/* -------------------------------------------------------------------------- */
SFTPTransfer::~SFTPTransfer()
    throw ()
{
    Debug::debug()->trace("SFTPTransfer::~SFTPTransfer()");

    if (m_process.getChildPID() != -1) {
	close(m_fdreq);
	close(m_fdresp);

	int status = m_process.wait();
	if (status != 0)
	    throw KError("SFTPTransfer::~SFTPTransfer: ssh command failed"
			 " with status " + Stringutil::number2string(status));
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
}

/* -------------------------------------------------------------------------- */
StringVector SFTPTransfer::makeArgs(void)
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

    ret.push_back("-s");

    ret.push_back(target.getHostname());
    ret.push_back("sftp");

    return ret;
}

//}}}

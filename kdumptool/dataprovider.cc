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
#include <unistd.h>

#include "dataprovider.h"
#include "global.h"
#include "progress.h"
#include "debug.h"
#include "stringutil.h"
#include "fileutil.h"

using std::fopen;
using std::fread;
using std::fclose;
using std::min;
using std::copy;
using std::string;

//{{{ AbstractDataProvider -----------------------------------------------------

// -----------------------------------------------------------------------------
AbstractDataProvider::AbstractDataProvider()
    throw ()
    : m_progress(NULL), m_error(false)
{}

// -----------------------------------------------------------------------------
bool AbstractDataProvider::canSaveToFile() const
    throw ()
{
    return false;
}

// -----------------------------------------------------------------------------
void AbstractDataProvider::saveToFile(const StringVector &targets)
    throw (KError)
{
    throw KError("That DataProvider cannot save to a file.");
}

// -----------------------------------------------------------------------------
void AbstractDataProvider::prepare()
    throw (KError)
{
    Debug::debug()->trace("AbstractDataProvider::prepare");
    if (m_progress)
        m_progress->start();
}

// -----------------------------------------------------------------------------
void AbstractDataProvider::finish()
    throw (KError)
{
    Debug::debug()->trace("AbstractDataProvider::finish");
    if (m_progress)
        m_progress->stop(!getError());
}

// -----------------------------------------------------------------------------
void AbstractDataProvider::setProgress(Progress *progress)
    throw ()
{
    Debug::debug()->trace("AbstractDataProvider::setProgress, p=%p", progress);
    m_progress = progress;
}

// -----------------------------------------------------------------------------
Progress *AbstractDataProvider::getProgress() const
    throw ()
{
    return m_progress;
}

// -----------------------------------------------------------------------------
void AbstractDataProvider::setError(bool error)
    throw ()
{
    m_error = error;
}

// -----------------------------------------------------------------------------
bool AbstractDataProvider::getError() const
    throw ()
{
    return m_error;
}

//}}}
//{{{ FileDataProvider ---------------------------------------------------------

// -----------------------------------------------------------------------------
FileDataProvider::FileDataProvider(const char *filename)
    throw ()
    : m_filename(filename)
    , m_file(NULL)
    , m_currentPos(0)
{}

// -----------------------------------------------------------------------------
void FileDataProvider::prepare()
    throw (KError)
{
    Debug::debug()->trace("FileDataProvider::prepare");

    m_file = fopen(m_filename.c_str(), "r");
    if (!m_file)
        throw KSystemError("Cannot open file " + m_filename, errno);

    // get the file size, we need to use the raw Unix interface
    // because the ftell() function does not support large files on i386
    int fd = fileno(m_file);
    if (fd < 0)
        throw KSystemError("fileno() failed with " + m_filename + ".", errno);

    m_fileSize = lseek(fd, 0, SEEK_END);
    if (m_fileSize == (off_t)-1)
        throw KSystemError("lseek() failed with " + m_filename + ".", errno);

    // and back to the beginning
    int ret = lseek(fd, 0, SEEK_SET);
    if (ret == (off_t)-1)
        throw KSystemError("lseek() failed with " + m_filename + ".", errno);

    AbstractDataProvider::prepare();
}

// -----------------------------------------------------------------------------
size_t FileDataProvider::getData(char *buffer, size_t maxread)
    throw (KError)
{
    if (!m_file)
        throw KError("File " + m_filename + " not opened.");

    errno = 0;
    size_t ret = fread(buffer, 1, maxread, m_file);
    if (ret == 0 && errno != 0) {
        setError(true);
        throw KSystemError("Error reading from " + m_filename + " at " +
            Stringutil::number2hex(m_currentPos), errno);
    }

    Progress *p = getProgress();
    if (p)
        p->progressed(lseek(fileno(m_file), 0, SEEK_CUR), m_fileSize);

    m_currentPos += ret;

    return ret;
}

// -----------------------------------------------------------------------------
void FileDataProvider::finish()
    throw (KError)
{
    Debug::debug()->trace("FileDataProvider::finish");

    if (m_file) {
        fclose(m_file);
        m_file = NULL;
    }
    AbstractDataProvider::finish();
}

//}}}
//{{{ BufferDataProvider -------------------------------------------------------

// -----------------------------------------------------------------------------
BufferDataProvider::BufferDataProvider(const ByteVector &data)
    throw ()
    : m_data(data), m_currentPos(0)
{}

// -----------------------------------------------------------------------------
size_t BufferDataProvider::getData(char *buffer, size_t maxread)
    throw (KError)
{
    size_t size = min((unsigned long long)maxread,
                      m_data.size() - m_currentPos);

    // end
    if (size <= 0)
        return 0;

    for (unsigned int i = 0; i < size; i++)
        buffer[i] = m_data[i+m_currentPos];

    m_currentPos += size;

    return size;
}

//}}}
//{{{ ProcessDataProvider ------------------------------------------------------

// -----------------------------------------------------------------------------
ProcessDataProvider::ProcessDataProvider(const char *pipe_cmdline,
                                         const char *direct_cmdline)
    throw ()
    : m_pipeCmdline(pipe_cmdline), m_directCmdline(direct_cmdline),
      m_processFile(NULL)
{
    Debug::debug()->trace("ProcessDataProvider::ProcessDataProvider(%s, %s)",
        pipe_cmdline, direct_cmdline);
}

// -----------------------------------------------------------------------------
void ProcessDataProvider::prepare()
    throw (KError)
{
    Debug::debug()->trace("ProcessDataProvider::prepare");

    m_processFile = popen(m_pipeCmdline.c_str(), "r");
    if (!m_processFile)
        throw KSystemError("Could not start process " + m_pipeCmdline, errno);
}

// -----------------------------------------------------------------------------
size_t ProcessDataProvider::getData(char *buffer, size_t maxread)
    throw (KError)
{
    if (!m_processFile)
        throw KError("Process " + m_pipeCmdline + " not started.");

    errno = 0;
    size_t ret = fread(buffer, 1, maxread, m_processFile);
    if (ret == 0 && errno != 0) {
        setError(true);
        throw KSystemError("Error reading from " + m_pipeCmdline, errno);
    }

    return ret;
}

// -----------------------------------------------------------------------------
void ProcessDataProvider::finish()
    throw (KError)
{
    Debug::debug()->trace("ProcessDataProvider::finish");

    int err = pclose(m_processFile);
    m_processFile = NULL;

    if (WEXITSTATUS(err) != 0)
        throw KError(m_pipeCmdline + " failed (" +
            Stringutil::number2string(WEXITSTATUS(err)) +").");
}

// -----------------------------------------------------------------------------
bool ProcessDataProvider::canSaveToFile() const
    throw ()
{
    return true;
}

// -----------------------------------------------------------------------------
void ProcessDataProvider::saveToFile(const StringVector &targets)
    throw (KError)
{
    Debug::debug()->trace("ProcessDataProvider::saveToFile([ \"%s\"%s ])",
	targets.front().c_str(), targets.size() > 1 ? ", ...": "");

    string cmdline = m_directCmdline;
    StringVector::const_iterator it;
    for (it = targets.begin(); it != targets.end(); ++it) {
	cmdline += ' ';
	cmdline += *it;
    }

    Debug::debug()->trace("Executing '%s'", cmdline.c_str());

    int err = system(cmdline.c_str());
    if (WEXITSTATUS(err) != 0)
        throw KError("Running " + m_directCmdline + " failed (" +
            Stringutil::number2string(WEXITSTATUS(err)) +").");
}

//}}}


// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

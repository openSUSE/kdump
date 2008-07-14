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

#include "dataprovider.h"
#include "global.h"
#include "debug.h"
#include "transfer.h"
#include "urlparser.h"
#include "util.h"
#include "fileutil.h"

using std::fopen;
using std::fread;
using std::fclose;
using std::string;

//{{{ FileTransfer -------------------------------------------------------------

// -----------------------------------------------------------------------------
void FileTransfer::perform(DataProvider *dataprovider,
                           const char *target_url,
                           const char *target_file)
    throw (KError)
{
    size_t buffersize;
    FILE *fp = open(target_url, target_file, &buffersize);

    char *buffer = new char[buffersize];
    bool prepared = false;

    try {
        dataprovider->prepare();
        prepared = true;

        while (true) {
            size_t read_data = dataprovider->getData(buffer, buffersize);

            // finished?
            if (read_data == 0)
                break;

            // sparse files
            if (read_data == buffersize && Util::isZero(buffer, buffersize)) {
                int ret = fseek(fp, buffersize, SEEK_CUR);
                if (ret != 0)
                    throw KSystemError("fseek() failed.", errno);
            } else {
                int ret = fwrite(buffer, buffersize, 1, fp);
                if (ret != 0)
                    throw KSystemError("write() failed.", errno);
            }
        }

        dataprovider->finish();

    } catch (...) {
        close(fp);
        if (prepared)
            dataprovider->finish();
        delete[] buffer;
        throw;
    }

    delete[] buffer;
    close(fp);
    dataprovider->finish();
}

// -----------------------------------------------------------------------------
FILE *FileTransfer::open(const char *target_url, const char *target_file,
                         size_t *buffersize)
    throw (KError)
{
    URLParser parser;
    parser.parseURL(target_url);

    Debug::debug()->trace("FileTransfer::open(%s, %s)",
        target_url, target_file);

    if (parser.getProtocol() != URLParser::PROT_FILE)
        throw KError("Only file URLs are allowed.");

    string dir = parser.getPath();
    FileUtil::mkdir(dir, true);

    string file = FileUtil::pathconcat(dir, target_file);

    FILE *fp = fopen(file.c_str(), "r");
    if (!fp)
        throw KSystemError("Error in fopen for " + file, errno);

    if (buffersize) {
        struct stat mystat;

        int err = fstat(fileno(fp), &mystat);
        if (err != 0)
            throw KSystemError("fstat() on " + dir + " failed.", errno);

        *buffersize = mystat.st_size;
        if (!*buffersize)
            *buffersize = BUFSIZ;
    }

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


// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

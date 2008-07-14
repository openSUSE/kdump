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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <libgen.h>

#include "dataprovider.h"
#include "global.h"
#include "progress.h"
#include "debug.h"
#include "fileutil.h"

using std::string;
using std::free;


//{{{ FileUtil -----------------------------------------------------------------

// -----------------------------------------------------------------------------
void FileUtil::mkdir(const std::string &dir, bool recursive)
    throw (KError)
{
    Debug::debug()->trace("mkdir(%s, %d)", dir.c_str(), int(recursive));

    if (!recursive) {
        if (!FileUtil::exists(dir)) {
            int ret = ::mkdir(dir.c_str(), 0755);
            if (ret != 0)
                throw KSystemError("mkdir of " + dir + " failed.", errno);
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
                FileUtil::mkdir(directory, false);
                break;
            }

            mkdir(directory.substr(0, current_slash), false);
        }
    }
}

// -----------------------------------------------------------------------------
bool FileUtil::exists(const string &file)
    throw ()
{
    struct stat mystat;
    int ret = stat(file.c_str(), &mystat);
    return ret == 0;
}

// -----------------------------------------------------------------------------
string FileUtil::basename(const string &file)
    throw ()
{
    // modification of the arguments is allowed
    char *path = strdup(file.c_str());

    const char *ret = ::basename(path);
    free(path);

    return string(ret);
}

// -----------------------------------------------------------------------------
string FileUtil::dirname(const string &file)
    throw ()
{
    // modification of the arguments is allowed
    char *path = strdup(file.c_str());

    const char *ret = ::dirname(path);
    free(path);

    return string(ret);
}

// -----------------------------------------------------------------------------
string FileUtil::pathconcat(const string &a, const string &b)
    throw ()
{
    return a + "/" + b;
}

//}}}


// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

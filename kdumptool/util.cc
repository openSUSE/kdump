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
#include <string>
#include <errno.h>
#include <sys/utsname.h>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "global.h"
#include "util.h"
#include "debug.h"

using std::string;
using std::strerror;

// -----------------------------------------------------------------------------
std::string Util::getArch()
    throw (KError)
{
    static struct utsname utsname;
    static bool utsname_filled = false;

    if (!utsname_filled) {
        int ret = uname(&utsname);
        if (ret < 0)
            throw KError(string("uname failed: ") + strerror(errno));
        utsname_filled = true;
    }

    Debug::debug()->dbg("uname = %s\n", utsname.machine);

    return utsname.machine;
}

// -----------------------------------------------------------------------------
bool Util::isGzipFile(const char *file)
    throw (KError)
{
    int             ret;
    int             fd = -1;
    unsigned char   buffer[2];

    fd = open(file, O_RDONLY);
    if (fd < 0)
        throw KSystemError("Util::isGzipFile: Couldn't open file", errno);

    /* read the first 2 bytes */
    ret = read(fd, buffer, 2);
    close(fd);
    if (ret < 0)
        throw KSystemError("Util::isGzipFile: read failed", errno);

    return buffer[0] == 0x1f && buffer[1] == 0x8b;
}

// -----------------------------------------------------------------------------
bool Util::isX86(const std::string &arch)
    throw ()
{
    return arch == "i386" || arch == "i686" ||
        arch == "i586" || arch == "x86_64";
}

// vim: set sw=4 ts=4 fdm=marker et:

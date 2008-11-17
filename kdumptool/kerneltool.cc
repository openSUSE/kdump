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
#include <cerrno>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "kerneltool.h"
#include "util.h"
#include "global.h"

using std::string;

/* x86 boot header for bzImage */
#define X86_HEADER_OFF_START        0x202
#define X86_HEADER_OFF_VERSION      0x0206
#define X86_HEADER_OFF_RELOCATABLE  0x0234
#define X86_HEADER_OFF_MAGIC        0x53726448
#define X86_HEADER_RELOCATABLE_VER  0x0205

// -----------------------------------------------------------------------------
KernelTool::KernelType KernelTool::getKernelType(const string &file)
    throw (KError)
{
    if (Util::isElfFile(file)) {
        if (Util::isGzipFile(file))
            return KT_ELF_GZ;
        else
            return KT_ELF;
    } else {
        if (!Util::isX86(Util::getArch()))
            return KT_NONE;
        else {
            if (isX86Kernel(file))
                return KT_X86;
            else
                return KT_NONE;
        }
    }
}

// -----------------------------------------------------------------------------
bool KernelTool::isX86Kernel(const string &file)
    throw (KError)
{
    int             fd = -1;
    int             ret;
    unsigned char   buffer[BUFSIZ];
    off_t           off_ret;

    fd = open(file.c_str(), O_RDONLY);
    if (fd < 0)
        throw KSystemError("IdentifyKernel::isX86Kernel: open failed", errno);

    /* check the magic number */
    off_ret = lseek(fd, X86_HEADER_OFF_START, SEEK_SET);
    if (off_ret == (off_t)-1) {
        close(fd);
        throw KSystemError("IdentifyKernel::isX86Kernel: lseek to "
            "X86_HEADER_OFF_START failed", errno);
    }

    ret = read(fd, buffer, 4);
    if (ret != 4) {
        close(fd);
        throw KSystemError("IdentifyKernel::isX86Kernel: read of magic "
            "start failed", errno);
    }

    close(fd);

    return *((uint32_t *)buffer) == X86_HEADER_OFF_MAGIC;
}

// -----------------------------------------------------------------------------
bool KernelTool::x86isRelocatable(const string &file)
    throw (KError)
{
    int             fd = -1;
    int             ret;
    unsigned char   buffer[BUFSIZ];
    off_t           off_ret;

    fd = open(file.c_str(), O_RDONLY);
    if (fd < 0)
        throw KSystemError("IdentifyKernel::checkArchFileX86: open failed",
            errno);

    // check the magic number
    off_ret = lseek(fd, X86_HEADER_OFF_START, SEEK_SET);
    if (off_ret == (off_t)-1) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: lseek to "
            "X86_HEADER_OFF_START failed", errno);
    }

    ret = read(fd, buffer, 4);
    if (ret != 4) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: read of magic "
            "start failed", errno);
    }

    if (*((uint32_t *)buffer) != X86_HEADER_OFF_MAGIC) {
        close(fd);
        throw KError("This is not a kernel image");
    }

    // check the version
    off_ret = lseek(fd, X86_HEADER_OFF_VERSION, SEEK_SET);
    if (off_ret == (off_t)-1) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: lseek to "
            "X86_HEADER_OFF_VERSION failed", errno);
    }

    ret = read(fd, buffer, 2);
    if (ret != 2) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: read of version"
            " failed", errno);
    }

    // older versions are not relocatable
    if (*((uint16_t *)buffer) < X86_HEADER_RELOCATABLE_VER) {
        close(fd);
        return false;
    }

    // and check if the kernel is compiled to be relocatable
    off_ret = lseek(fd, X86_HEADER_OFF_RELOCATABLE, SEEK_SET);
    if (off_ret == (off_t)-1) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: lseek to "
            "X86_HEADER_OFF_RELOCATABLE failed", errno);
    }

    ret = read(fd, buffer, 1);
    if (ret != 1) {
        close(fd);
        throw KSystemError("IdentifyKernel::checkArchFileX86: read of "
            "relocatable bit failed", errno);
    }

    close(fd);

    return !!buffer[0];
}


// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

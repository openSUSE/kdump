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
#include <stdexcept>
#include <cstring>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>

#include "global.h"

#if HAVE_LIBSSH2
#   include <libssh2.h>
#   include <libssh2_sftp.h>
#endif
#include <gelf.h>

#define MAXERROR 4096

using std::strerror;
using std::string;

//{{{ KSystemError -------------------------------------------------------------

// -----------------------------------------------------------------------------
const char *KSystemError::what() const
    throw ()
{
    static char buffer[MAXERROR];

    string errorstring = m_errorstring + " (" + strerror(m_errorcode) + ")";

    strncpy(buffer, errorstring.c_str(), MAXERROR);
    buffer[MAXERROR-1] = 0;

    return buffer;
}

//}}}
//{{{ KNetError ----------------------------------------------------------------

/* -------------------------------------------------------------------------- */
const char *KNetError::what() const
    throw ()
{
    static char buffer[MAXERROR];

    string errorstring = m_errorstring + " (" + hstrerror(m_errorcode) + ")";

    strncpy(buffer, errorstring.c_str(), MAXERROR);
    buffer[MAXERROR-1] = 0;

    return buffer;
}

//}}}
//{{{ KSFTPError ---------------------------------------------------------------

#if HAVE_LIBSSH2
/* -------------------------------------------------------------------------- */
const char *KSFTPError::what() const
    throw ()
{
    static char buffer[MAXERROR];

    string errorstring = m_errorstring + " (" +
            getStringForCode(m_errorcode) + ")";

    strncpy(buffer, errorstring.c_str(), MAXERROR);
    buffer[MAXERROR-1] = 0;

    return buffer;
}

/* -------------------------------------------------------------------------- */
string KSFTPError::getStringForCode(int code) const
    throw ()
{
    const char *msg;

    switch (code) {
        case LIBSSH2_FX_OK:
            msg = "OK";

        case LIBSSH2_FX_EOF:
            msg = "End of file";

        case LIBSSH2_FX_NO_SUCH_FILE:
            msg = "No such file";

        case LIBSSH2_FX_PERMISSION_DENIED:
            msg = "Permission denied";

        case LIBSSH2_FX_FAILURE:
            msg = "Failure";

        case LIBSSH2_FX_BAD_MESSAGE:
            msg = "Bad message";

        case LIBSSH2_FX_NO_CONNECTION:
            msg = "No connection";

        case LIBSSH2_FX_CONNECTION_LOST:
            msg = "Connection lost";

        case LIBSSH2_FX_OP_UNSUPPORTED:
            msg = "Operation unsupported";

        case LIBSSH2_FX_INVALID_HANDLE:
            msg = "Invalid handle";

        case LIBSSH2_FX_NO_SUCH_PATH:
            msg = "No such path";

        case LIBSSH2_FX_FILE_ALREADY_EXISTS:
            msg = "File already exists";

        case LIBSSH2_FX_WRITE_PROTECT:
            msg = "Write protect";

        case LIBSSH2_FX_NO_MEDIA:
            msg = "No media";

        case LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM:
            msg = "No space on file system";

        case LIBSSH2_FX_QUOTA_EXCEEDED:
            msg = "Quote exceeded";

        case LIBSSH2_FX_UNKNOWN_PRINCIPLE:
            msg = "Unknown principle";

        case LIBSSH2_FX_LOCK_CONFLICT:
            msg = "Lock conflict";

        case LIBSSH2_FX_DIR_NOT_EMPTY:
            msg = "Directory not empty";

        case LIBSSH2_FX_NOT_A_DIRECTORY:
            msg = "Not a directory";

        case LIBSSH2_FX_INVALID_FILENAME:
            msg = "Invalid file name";

        case LIBSSH2_FX_LINK_LOOP:
            msg = "Link loop";

        default:
            msg = "Unknown error";
    }

    return string(msg);
}

#endif // HAVE_LIBSSH2

//}}}
//{{{ KELFError ----------------------------------------------------------------

/* -------------------------------------------------------------------------- */
const char *KELFError::what() const
    throw ()
{
    static char buffer[MAXERROR];

    string errorstring = m_errorstring + " (" + elf_errmsg(m_errorcode) + ")";

    strncpy(buffer, errorstring.c_str(), MAXERROR);
    buffer[MAXERROR-1] = 0;

    return buffer;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

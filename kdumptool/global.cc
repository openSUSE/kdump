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
#include <cerrno>
#include <sys/socket.h>
#include <netdb.h>

#include "global.h"

#if HAVE_LIBSSH2
#   include <libssh2.h>
#   include <libssh2_sftp.h>
#endif
#if HAVE_LIBESMTP
#   include <libesmtp.h>
#endif

#include <gelf.h>

#define MAXERROR 4096

using std::strerror;
using std::string;

//{{{ KSystemErrorCode ---------------------------------------------------------

// -----------------------------------------------------------------------------
string KSystemErrorCode::message(void) const
    throw ()
{
    return string(strerror(getCode()));
}

//}}}
//{{{ KNetErrorCode ------------------------------------------------------------

// -----------------------------------------------------------------------------
string KNetErrorCode::message(void) const
    throw ()
{
    return string(hstrerror(getCode()));
}

//}}}
//{{{ KGaiErrorCode ------------------------------------------------------------

// -----------------------------------------------------------------------------
string KGaiErrorCode::message(void) const
    throw ()
{
    return string(gai_strerror(getCode()));
}

//}}}
//{{{ KSFTPErrorCode -----------------------------------------------------------

/* -------------------------------------------------------------------------- */
string KSFTPErrorCode::message(void) const
    throw ()
{
#if HAVE_LIBSSH2
    const char *msg;

    switch (getCode()) {
        case LIBSSH2_FX_OK:
            msg = "OK";
            break;

        case LIBSSH2_FX_EOF:
            msg = "End of file";
            break;

        case LIBSSH2_FX_NO_SUCH_FILE:
            msg = "No such file";
            break;

        case LIBSSH2_FX_PERMISSION_DENIED:
            msg = "Permission denied";
            break;

        case LIBSSH2_FX_FAILURE:
            msg = "Failure";
            break;

        case LIBSSH2_FX_BAD_MESSAGE:
            msg = "Bad message";
            break;

        case LIBSSH2_FX_NO_CONNECTION:
            msg = "No connection";
            break;

        case LIBSSH2_FX_CONNECTION_LOST:
            msg = "Connection lost";
            break;

        case LIBSSH2_FX_OP_UNSUPPORTED:
            msg = "Operation unsupported";
            break;

        case LIBSSH2_FX_INVALID_HANDLE:
            msg = "Invalid handle";
            break;

        case LIBSSH2_FX_NO_SUCH_PATH:
            msg = "No such path";
            break;

        case LIBSSH2_FX_FILE_ALREADY_EXISTS:
            msg = "File already exists";
            break;

        case LIBSSH2_FX_WRITE_PROTECT:
            msg = "Write protect";
            break;

        case LIBSSH2_FX_NO_MEDIA:
            msg = "No media";
            break;

        case LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM:
            msg = "No space on file system";
            break;

        case LIBSSH2_FX_QUOTA_EXCEEDED:
            msg = "Quote exceeded";
            break;

        case LIBSSH2_FX_UNKNOWN_PRINCIPLE:
            msg = "Unknown principle";
            break;

        case LIBSSH2_FX_LOCK_CONFLICT:
            msg = "Lock conflict";
            break;

        case LIBSSH2_FX_DIR_NOT_EMPTY:
            msg = "Directory not empty";
            break;

        case LIBSSH2_FX_NOT_A_DIRECTORY:
            msg = "Not a directory";
            break;

        case LIBSSH2_FX_INVALID_FILENAME:
            msg = "Invalid file name";
            break;

        case LIBSSH2_FX_LINK_LOOP:
            msg = "Link loop";
            break;

        default:
            msg = "Unknown error";
    }
    return string(msg);

#else // HAVE_LIBSSH2
    return string("Compiled without libssh2 support");

#endif // HAVE_LIBSSH2
}
//}}}
//{{{ KELFErrorCode ------------------------------------------------------------

// -----------------------------------------------------------------------------
string KELFErrorCode::message(void) const
    throw ()
{
    return string(elf_errmsg(getCode()));
}

//}}}
//{{{ KSmtpErrorCode -----------------------------------------------------------

// -----------------------------------------------------------------------------
string KSmtpErrorCode::message(void) const
    throw ()
{
#if HAVE_LIBESMTP
    char smtp_buffer[MAXERROR];

    smtp_strerror(getCode(), smtp_buffer, MAXERROR);
    smtp_buffer[MAXERROR-1] = 0;
    return string(smtp_buffer);

#else // HAVE_LIBESMTP
    return string("Compiled without libesmtp support");

#endif // HAVE_LIBESMTP
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

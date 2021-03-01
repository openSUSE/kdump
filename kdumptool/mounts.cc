/*
 * (c) 2014, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
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

#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <unistd.h>

// for makedev() and friends:
#include <sys/sysmacros.h>

#include "global.h"
#include "debug.h"
#include "mounts.h"
#include "process.h"

using std::string;
using std::ifstream;
using std::istringstream;
using std::ostringstream;

//{{{ MountPoint ---------------------------------------------------------------

// -----------------------------------------------------------------------------
MountPoint& MountPoint::operator=(MountPoint const& other)
{
    m_fs = other.m_fs;
    m_ctarget = other.m_ctarget;
    return *this;
}

// -----------------------------------------------------------------------------
void MountPoint::setFS(MntFS const& fs)
{
    m_fs = fs;
    m_ctarget.clear();
}

// -----------------------------------------------------------------------------
FilePath const& MountPoint::canonicalTarget(void)
{
    if (m_ctarget.empty()) {
        FilePath target = mnt_fs_get_target(m_fs);
        m_ctarget = target.getCanonicalPath();
    }
    return m_ctarget;
}

//}}}
//{{{ MountTable ---------------------------------------------------------------

// -----------------------------------------------------------------------------
MountTable::iterator::iterator(MountTable const& table, int direction)
    : m_tb(table.m_tb), m_mp(NULL)
{
    m_it = mnt_new_iter(MNT_ITER_FORWARD);
    if (!m_it)
        throw KError("Failed to initialize libmount iterator");
}

// -----------------------------------------------------------------------------
MountTable::iterator::iterator(iterator const& other)
    : m_tb(other.m_tb), m_mp(other.m_mp)
{
    m_it = mnt_new_iter(other.getDirection());
    if (!m_it)
        throw KError("Failed to initialize libmount iterator");
    mnt_table_set_iter(m_tb, m_it, m_mp.fs());
}

// -----------------------------------------------------------------------------
MountTable::iterator::~iterator()
{
    mnt_free_iter(m_it);
}

// -----------------------------------------------------------------------------
MountTable::iterator& MountTable::iterator::operator=(iterator const& other)
{
    m_tb = other.m_tb;
    m_mp = other.m_mp;
    mnt_table_set_iter(m_tb, m_it, m_mp.fs());
    return *this;
}

// -----------------------------------------------------------------------------
MountTable::iterator& MountTable::iterator::operator++()
{
    struct libmnt_fs *fs;
    if (mnt_table_next_fs(m_tb, m_it, &fs) < 0)
        throw KError("Failed to iterate mount table");
    m_mp.setFS(fs);
    return *this;
}

// -----------------------------------------------------------------------------
MountTable::MountTable(void)
    : m_tb(mnt_new_table())
{
    if (!m_tb)
        throw KError("Failed to initialize libmount table");
}

// -----------------------------------------------------------------------------
MountTable::iterator MountTable::find_mount(FilePath const& path)
{
    Debug::debug()->trace("MountTable::find_mount(%s)", path.c_str());

    FilePath cpath = path.getCanonicalPath();
    size_t bestlen = 0;
    iterator it(*this, MNT_ITER_FORWARD);
    iterator best = it;
    while (++it) {
        if (it->isPseudoFS() || it->isSwapArea())
            continue;

        FilePath const& ctarget = it->canonicalTarget();
        if (ctarget.length() >= bestlen &&
            cpath.compare(0, ctarget.length(), ctarget) == 0) {
            best = it;
            bestlen = ctarget.length();
        }
    }
    return best;
}

//}}}
//{{{ KernelMountTable ---------------------------------------------------------

const char KernelMountTable::_PATH_PROC_MOUNTS[] = "/proc/mounts";
const char KernelMountTable::_PATH_PROC_MOUNTINFO[] = "/proc/self/mountinfo";

// -----------------------------------------------------------------------------
KernelMountTable::KernelMountTable(void)
    : MountTable()
{
    const char *path = access(_PATH_PROC_MOUNTINFO, R_OK) == 0
        ? _PATH_PROC_MOUNTINFO
        : _PATH_PROC_MOUNTS;

    if (mnt_table_parse_file(m_tb, path) != 0)
        throw KError("Can't read kernel mount table");
}

//}}}
//{{{ FstabMountTable ----------------------------------------------------------

// -----------------------------------------------------------------------------
FstabMountTable::FstabMountTable(void)
    : MountTable()
{
    if (mnt_table_parse_fstab(m_tb, NULL) != 0)
        throw KError("Can't read fstab");
}

//}}}
//{{{ PathMountPoint -----------------------------------------------------------

// -----------------------------------------------------------------------------
PathMountPoint::PathMountPoint(FilePath const& path)
    : MountPoint(NULL)
{
    Debug::debug()->trace("PathMountPoint::PathMountPoint(%s)", path.c_str());

    MountTable::iterator mp_kernel =
        KernelMountTable().find_mount(path);
    MountTable::iterator mp_fstab =
        FstabMountTable().find_mount(path);
    MountTable::iterator *best;

    if (!mp_fstab) {
        Debug::debug()->dbg("%s: No fstab entry", path.c_str());
        best = &mp_kernel;
    } else if (!mp_kernel) {
        Debug::debug()->dbg("%s: No kernel entry", path.c_str());
        best = &mp_fstab;
    } else {
        Debug::debug()->dbg("%s: Kernel entry: %s, fstab entry: %s",
                            path.c_str(),
                            mp_kernel->canonicalTarget().c_str(),
                            mp_fstab->canonicalTarget().c_str());

        // both exist: choose the longer one, or fstab if same length
        if (mp_kernel->canonicalTarget().length() >
            mp_fstab->canonicalTarget().length()) {
            best = &mp_kernel;
        } else {
            best = &mp_fstab;
        }
    }
    MountPoint::operator=(**best);

    if (*best) {
        Debug::debug()->dbg("Filesystem on %s mounted at %s",
                            source(), target());
    } else {
        Debug::debug()->dbg("No filesystem found!");
    }
}

//}}}
//{{{ PathResolver -------------------------------------------------------------

const char PathResolver::_PATH_DEV_BLOCK_[] = "/dev/block/";
const char PathResolver::_PATH_DEV_ROOT[] = "/dev/root";
const char PathResolver::_PATH_PROC_CMDLINE[] = "/proc/cmdline";

FilePath PathResolver::m_devroot;

// -----------------------------------------------------------------------------
PathResolver::PathResolver()
{
    m_cache = mnt_new_cache();
    if (!m_cache)
        throw KError("Cannot allocate file path cache");
}

// -----------------------------------------------------------------------------
PathResolver::~PathResolver()
{
    mnt_free_cache(m_cache);
}

// -----------------------------------------------------------------------------
bool PathResolver::_devroot_maj_min(void)
{
    size_t pos = m_devroot.find(':');
    if (pos == string::npos)
        return false;
    KString devmajor(m_devroot, 0, pos);
    if (!devmajor.isNumber())
        return false;

    KString devminor(m_devroot, pos + 1);
    pos = devminor.find(':');
    if (pos != string::npos) {
        KString rest(devminor, pos + 1);
        if (!rest.isNumber())
            return false;
        devminor.resize(pos);
    }
    if (!devminor.isNumber())
        return false;

    ostringstream os;
    os << _PATH_DEV_BLOCK_ << devmajor << ':' << devminor;
    m_devroot = os.str();
    return true;
}

// -----------------------------------------------------------------------------
bool PathResolver::_devroot_hexhex(void)
{
    dev_t dev;
    if (!m_devroot.isHexNumber())
        return false;

    istringstream is(m_devroot);
    is >> std::hex >> dev;

    ostringstream os;
    os << _PATH_DEV_BLOCK_ << major(dev) << ':' << minor(dev);
    m_devroot = os.str();
    return true;
}

// -----------------------------------------------------------------------------
FilePath& PathResolver::system_root(void)
{
    Debug::debug()->trace("PathResolver::system_root()");

    if (m_devroot.empty()) {
        ifstream fin(_PATH_PROC_CMDLINE);
        if (!fin)
            throw KError(string("Unable to open ") + _PATH_PROC_CMDLINE + ".");

        const string param = "root=";
        while (fin.good()) {
            bool in_quote = false;
            KString s;
            char c;
            while (fin.get(c) && !(!in_quote && isspace(c))) {
                s += c;
                if (c == '"')
                    in_quote = !in_quote;
            }

            // remove surrounding quotes
            if (!s.empty() && *s.begin() == '"') {
                s.erase(s.begin());
                string::iterator last = s.end();
                if (last != s.begin() && *--last == '"')
                    s.erase(last);
            }

            if (s == "--")
                break;

            if (s.startsWith(param))
                m_devroot.assign(s, param.length());
        }
        fin.close();

        if (!m_devroot.empty()) {
            Debug::debug()->dbg("Kernel commandline says root=%s",
                                m_devroot.c_str());
            if (!_devroot_maj_min() &&
                !_devroot_hexhex()) {
                m_devroot = resolve(m_devroot);
            }
            Debug::debug()->dbg("System root resolved as %s",
                                m_devroot.c_str());
        } else {
            Debug::debug()->dbg("System root cannot be determined");
            m_devroot = _PATH_DEV_ROOT;
        }
    }
    return m_devroot;
}

// -----------------------------------------------------------------------------
FilePath PathResolver::resolve(string const& spec)
{
    Debug::debug()->trace("PathResolver::resolve(%s)", spec.c_str());

    char *path = mnt_resolve_spec(spec.c_str(), m_cache);
    if (!path)
        throw KError("Cannot resolve path: " + spec);
    FilePath ret = path;
    if (ret == _PATH_DEV_ROOT)
        return system_root();
    return ret;
}

//}}}
//{{{ FilesystemTypeMap --------------------------------------------------------

// -----------------------------------------------------------------------------
void FilesystemTypeMap::addPath(FilePath const& path)
{
    Debug::debug()->trace("FilesystemTypeMap::addPath(%s)", path.c_str());

    PathMountPoint mnt(path);
    if (mnt)
        m_sources.insert(m_resolver.resolve(mnt.source()));
}

// -----------------------------------------------------------------------------
StringStringMap& FilesystemTypeMap::devices(void)
{
    Debug::debug()->trace("FilesystemTypeMap::devices()");

    if (m_sources.empty())
        return m_devices;

    ProcessFilter p;

    StringVector args;
    static const char *const opts[] = {
        "--raw", "--noheadings", "--inverse",
        "--output", "FSTYPE,PATH",
        NULL
    };
    for (const char *const *p = opts; *p; ++p)
        args.push_back(*p);
    args.insert(args.end(), m_sources.begin(), m_sources.end());

    ostringstream stdoutStream, stderrStream;
    p.setStdout(&stdoutStream);
    p.setStderr(&stderrStream);
    int ret = p.execute("lsblk", args);
    if (ret != 0) {
        KString error = stderrStream.str();
        throw KError("lsblk failed: " + error.trim());
    }

    KString out = stdoutStream.str();
    size_t pos = 0;
    while (pos < out.length()) {
        size_t end = out.find_first_of("\r\n", pos);
        size_t sep = out.find(' ', pos);
        if (sep < end) {
            string type = out.substr(pos, sep - pos);
            string path = out.substr(sep + 1, end - sep - 1);
            m_devices[path] = type;

            Debug::debug()->dbg("Device %s type %s",
                                path.c_str(), type.c_str());
        }
        pos = out.find_first_not_of("\r\n", end);
    }

    return m_devices;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

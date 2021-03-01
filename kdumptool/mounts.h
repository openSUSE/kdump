/*
 * (c) 2021, Petr Tesarik <ptesarik@suse.de>, SUSE Linux Software Solutions GmbH
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
#ifndef MOUNTS_H
#define MOUNTS_H

#include <set>

#include "global.h"
#include "fileutil.h"

#include <libmount.h>

//{{{ MntFS --------------------------------------------------------------------

/**
 * Refcounting wrapper for struct libmnt_fs*
 */
class MntFS {
        struct libmnt_fs *m_fs;

    public:
        MntFS(struct libmnt_fs *fs)
            : m_fs(fs)
        { mnt_ref_fs(m_fs); }

        MntFS(MntFS const& other)
            : m_fs(other.m_fs)
        { mnt_ref_fs(m_fs); }

        ~MntFS()
        { mnt_unref_fs(m_fs); }

        /**
         * Typecast to struct libmnt_fs*, so instances of this class
         * can be used as arguments to libmount functions.
         */
        operator struct libmnt_fs*() const
        { return m_fs; }

        /**
         * Assign a new value, taking care of rerence counts.
         */
        MntFS& operator=(MntFS const& other)
        {
            mnt_unref_fs(m_fs);
            m_fs = other.m_fs;
            mnt_ref_fs(m_fs);
            return *this;
        }
};

//}}}

//{{{ MntTable -----------------------------------------------------------------

/*
 * Refcounting wrapper for struct libmnt_table*
 */
class MntTable {
        struct libmnt_table *m_tb;

    public:
        MntTable(struct libmnt_table *tb)
            : m_tb(tb)
        { mnt_ref_table(m_tb); }

        MntTable(MntTable const& other)
            : m_tb(other.m_tb)
        { mnt_ref_table(m_tb); }

        ~MntTable()
        { mnt_unref_table(m_tb); }

        /**
         * Typecast to struct libmnt_table*, so instances of this class
         * can be used as arguments to libmount functions.
         */
        operator struct libmnt_table*() const
        { return m_tb; }

        /**
         * Assign a new value, taking care of rerence counts.
         */
        MntTable& operator=(MntTable const& other)
        {
            mnt_unref_table(m_tb);
            m_tb = other.m_tb;
            mnt_ref_table(m_tb);
            return *this;
        }
};

//}}}

//{{{ MountPoint ---------------------------------------------------------------

class MountPoint {
    protected:
        MntFS m_fs;

    private:
        FilePath m_ctarget;

    public:
        MountPoint(const MntFS& fs)
            : m_fs(fs)
        { }

        MountPoint(MountPoint const& other)
            : m_fs(other.m_fs), m_ctarget(other.m_ctarget)
        { }

        operator bool() const
        { return (bool)m_fs; }

        MountPoint& operator=(MountPoint const& other);

        MntFS& fs(void)
        { return m_fs; }

        void setFS(MntFS const& fs);

        FilePath const& canonicalTarget(void);

        bool isNetFS(void)
        { return mnt_fs_is_netfs(m_fs); }

        bool isPseudoFS(void)
        { return mnt_fs_is_pseudofs(m_fs); }

        bool isSwapArea(void)
        { return mnt_fs_is_swaparea(m_fs); }

        const char *source(void)
        { return mnt_fs_get_source(m_fs); }

        const char *target(void)
        { return mnt_fs_get_target(m_fs); }

        const char *fstype(void)
        { return mnt_fs_get_fstype(m_fs); }
};

//}}}
//{{{ MountTable ---------------------------------------------------------------

class MountTable {
    protected:
        MntTable m_tb;

    public:
        class iterator {
            struct libmnt_iter *m_it;
            MntTable m_tb;
            MountPoint m_mp;

        public:
            iterator(MountTable const& table, int direction);
            iterator(iterator const& other);
            ~iterator();

            iterator& operator=(iterator const& other);

            operator bool() const
            { return (bool)m_mp; }

            iterator& operator++();
            iterator operator++(int)
            { iterator temp = *this; ++(*this); return temp; }

            MountPoint const& operator*() const { return m_mp; }
            MountPoint* operator->() { return &m_mp; }

            int getDirection(void) const
            { return mnt_iter_get_direction(m_it); }
        };

        MountTable(void);

        iterator find_mount(FilePath const& path);
};

//}}}
//{{{ KernelMountTable ---------------------------------------------------------

class KernelMountTable : public MountTable {
    public:
        KernelMountTable(void);

    private:
        static const char _PATH_PROC_MOUNTS[];
        static const char _PATH_PROC_MOUNTINFO[];
};

//}}}
//{{{ FstabMountTable ----------------------------------------------------------

class FstabMountTable : public MountTable {
    public:
        FstabMountTable(void);
};

//}}}
//{{{ PathMountPoint -----------------------------------------------------------

class PathMountPoint : public MountPoint {
    public:
        PathMountPoint(FilePath const& path);
        PathMountPoint(PathMountPoint const &other)
            : MountPoint(other)
        { }
};

//}}}
//{{{ PathResolver -------------------------------------------------------------

class PathResolver {
    protected:
        struct libmnt_cache *m_cache;

    public:
        PathResolver();
        ~PathResolver();

        FilePath resolve(std::string const& spec);
        FilePath& system_root(void);

    private:
        static const char _PATH_DEV_BLOCK_[];
        static const char _PATH_DEV_ROOT[];
        static const char _PATH_PROC_CMDLINE[];
        static FilePath m_devroot;

        bool _devroot_maj_min(void);
        bool _devroot_hexhex(void);
};

//}}}
//{{{ FilesystemTypeMap --------------------------------------------------------

class FilesystemTypeMap {
    protected:
        PathResolver m_resolver;
        std::set<std::string> m_sources;
        StringStringMap m_devices;

    public:
        void addPath(FilePath const& path);
        StringStringMap& devices(void);
};

//}}}

#endif /* MOUNTS_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

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
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <libgen.h>
#include <dirent.h>
#include <sys/vfs.h>

#include "dataprovider.h"
#include "global.h"
#include "progress.h"
#include "debug.h"
#include "fileutil.h"
#include "process.h"
#include "stringutil.h"

using std::string;
using std::free;


//{{{ FileUtil -----------------------------------------------------------------

// -----------------------------------------------------------------------------
void FileUtil::chroot(const std::string &dir)
    throw (KError)
{
    Debug::debug()->trace("chroot(%s)", dir.c_str());

    int ret = ::chroot(dir.c_str());
    if (ret < 0) {
        throw KSystemError("chroot failed", errno);
    }
}

// -----------------------------------------------------------------------------
void FileUtil::mkdir(const std::string &dir, bool recursive)
    throw (KError)
{
    Debug::debug()->trace("mkdir(%s, %d)", dir.c_str(), int(recursive));

    if (!recursive) {
        if (!FileUtil::exists(dir)) {
            Debug::debug()->dbg("::mkdir(%s)", dir.c_str());
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
bool FileUtil::isSymlink(const std::string &path)
    throw (KError)
{
    struct stat mystat;

    Debug::debug()->trace("isSymlink(%s)", path.c_str());

    int ret = lstat(path.c_str(), &mystat);
    if (ret < 0) {
        throw KSystemError("Stat failed", errno);
    }

    return S_ISLNK(mystat.st_mode);
}

// -----------------------------------------------------------------------------
string FileUtil::readlink(const std::string &path)
    throw (KError)
{
    Debug::debug()->trace("readlink(%s)", path.c_str());
    char buffer[BUFSIZ];

    int ret = ::readlink(path.c_str(), buffer, BUFSIZ-1);
    if (ret < 0) {
        throw KSystemError("readlink() failed", errno);
    }

    buffer[ret] = '\0';
    return string(buffer);
}

// -----------------------------------------------------------------------------
string FileUtil::getCanonicalPath(const string &path)
    throw (KError)
{
    Debug::debug()->trace("getCanonicalPath(%s)", path.c_str());

    char * ret = canonicalize_file_name(path.c_str());
    if (ret == NULL) {
        throw KSystemError("realpath() failed in getCanonicalPath()", errno);
    }

    string retstr = string(ret);
    free(ret);

    return retstr;
}

// -----------------------------------------------------------------------------
string FileUtil::getCanonicalPathRoot(const string &path, const string &root)
    throw (KError)
{
    if (root.size() == 0) {
        return getCanonicalPath(path);
    }

    Debug::debug()->trace("getCanonicalPathRoot(%s, %s)",
        path.c_str(), root.c_str());

    // check the permission first
    if (getuid() != 0) {
        throw KError("You have to be root to use that functionality.");
    }

    chroot(root);

    string result;
    try {
        result = getCanonicalPath(path);
    } catch (const KError &kerr) {
        try {
            chroot("..");
        } catch (const KError &err) {}
        throw;
    }

    chroot("..");
    return result;
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
string FileUtil::baseName(const string &file)
    throw ()
{
    // modification of the arguments is allowed
    char *path = strdup(file.c_str());
    string ret(::basename(path));
    free(path);
    return ret;
}

// -----------------------------------------------------------------------------
string FileUtil::dirname(const string &file)
    throw ()
{
    // modification of the arguments is allowed
    char *path = strdup(file.c_str());
    string ret(::dirname(path));
    free(path);
    return ret;
}

// -----------------------------------------------------------------------------
string FileUtil::pathconcat(const string &a, const string &b)
    throw ()
{
    return Stringutil::rtrim(a, "/") + PATH_SEPARATOR +
           Stringutil::ltrim(b, "/");
}

// -----------------------------------------------------------------------------
string FileUtil::pathconcat(const string &a, const string &b, const string &c)
    throw ()
{
    return Stringutil::rtrim(a, "/") + PATH_SEPARATOR +
           Stringutil::trim(b, "/") + PATH_SEPARATOR +
           Stringutil::ltrim(c, "/");
}

// -----------------------------------------------------------------------------
string FileUtil::pathconcat(const string &a,
                            const string &b,
                            const string &c,
                            const string &d)
    throw ()
{
    return Stringutil::rtrim(a, "/") + PATH_SEPARATOR +
           Stringutil::trim(b, "/") + PATH_SEPARATOR +
           Stringutil::trim(c, "/") + PATH_SEPARATOR +
           Stringutil::ltrim(d, "/");
}

// -----------------------------------------------------------------------------
void FileUtil::nfsmount(const string &host,
                        const string &dir,
                        const string &mountpoint,
                        const StringVector &options,
                        string &mountdir)
    throw (KError)
{
    Debug::debug()->trace("FileUtil::nfsmount(%s, %s, %s, %s)",
        host.c_str(), dir.c_str(), mountpoint.c_str(),
        Stringutil::vector2string(options).c_str());

    // get the exported file systems
    Process p;
    StringVector args;
    args.push_back("-e");
    args.push_back("--no-headers");
    args.push_back(host);
    ByteVector stdoutBytes, stderrBytes;
    p.setStdoutBuffer(&stdoutBytes);
    p.setStderrBuffer(&stderrBytes);
    int ret = p.execute("showmount", args);
    if (ret != 0)
        throw KError("Cannot execute showmount -e --no-headers " + host + ":" +
            Stringutil::trim(Stringutil::bytes2str(stderrBytes)));

    StringVector dirs = Stringutil::splitlines(
        Stringutil::bytes2str(stdoutBytes));

    mountdir = "";
    for (StringVector::const_iterator it = dirs.begin();
            it != dirs.end(); ++it) {

        // XXX: export cannot contain white spaces
        StringVector vec = Stringutil::split(*it, ' ');
        if (vec.size() < 1)
            throw KError("Invalid line in showmount output: " + *it);

        string exportdir = vec[0];

        Debug::debug()->trace("Checking %s", exportdir.c_str());
        if (Stringutil::startsWith(dir, exportdir)) {
            mountdir = exportdir;
            break;
        }
    }

    if (mountdir.size() == 0)
        throw KError(host + " does not export " + dir + ".");

    mount(host + ":" + mountdir, mountpoint, "nfs", options);

}

// -----------------------------------------------------------------------------
void FileUtil::mount(const std::string &device, const std::string &mountpoint,
                     const std::string &fs, const StringVector &options)
    throw (KError)
{
    StringVector args;

    Debug::debug()->trace("FileUtil::mount(%s %s, %s, %s)",
        device.c_str(), mountpoint.c_str(), fs.c_str(),
        Stringutil::vector2string(options).c_str());

    for (StringVector::const_iterator it = options.begin();
            it != options.end(); ++it) {
        args.push_back("-o");
        args.push_back(*it);
    }

    args.push_back("-t");
    args.push_back(fs);

    args.push_back(device);
    args.push_back(mountpoint);

    Process p;
    ByteVector stderrBuffer;
    p.setStderrBuffer(&stderrBuffer);

    int ret = p.execute("mount", args);
    Debug::debug()->dbg("Mount:%d", ret);
    if (ret != 0) {
        string error = Stringutil::trim(Stringutil::bytes2str(stderrBuffer));
        throw KError("mount failed: " + error + ".");
    }
}

// -----------------------------------------------------------------------------
void FileUtil::umount(const std::string &mountpoint)
    throw (KError)
{
    StringVector args;
    args.push_back(mountpoint);

    Debug::debug()->trace("FileUtil::umount(%s)", mountpoint.c_str());

    Process p;
    ByteVector stderrBuffer;
    p.setStderrBuffer(&stderrBuffer);

    int ret = p.execute("umount", args);
    if (ret != 0) {
        string error = Stringutil::trim(Stringutil::bytes2str(stderrBuffer));
        throw KError("umount failed: " + error);
    }
}

// -----------------------------------------------------------------------------
static int filter_dots(const struct dirent *d)
{
    if (strcmp(d->d_name, ".") == 0)
        return 0;
    if (strcmp(d->d_name, "..") == 0)
        return 0;
    else
        return 1;
}

// -----------------------------------------------------------------------------
static int filter_dots_and_nondirs(const struct dirent *d)
{
    if (strcmp(d->d_name, ".") == 0)
        return 0;
    if (strcmp(d->d_name, "..") == 0)
        return 0;
    else
        return d->d_type == DT_DIR;
}

// -----------------------------------------------------------------------------
StringVector FileUtil::listdir(const std::string &dir, bool onlyDirs)
    throw (KError)
{
    Debug::debug()->trace("FileUtil::listdir(%s)", dir.c_str());

    int (*filterfunction)(const struct dirent *);
    if (onlyDirs)
        filterfunction = filter_dots_and_nondirs;
    else
        filterfunction = filter_dots;

    StringVector v;
    struct dirent **namelist;
    int count = scandir(dir.c_str(), &namelist, filter_dots, alphasort);
    if (count < 0)
        throw KSystemError("Cannot scan directory " + dir + ".", errno);

    for (int i = 0; i < count; i++) {
        v.push_back(namelist[i]->d_name);
        free(namelist[i]);
    }
    free(namelist);

    return v;
}

// -----------------------------------------------------------------------------
void FileUtil::rmdir(const std::string &dir, bool recursive)
    throw (KError)
{
    Debug::debug()->trace("FileUtil::rmdir(%s, %d)", dir.c_str(), recursive);

    if (recursive) {
        DIR *dirptr = opendir(dir.c_str());
        if (!dirptr)
            throw KSystemError("Cannot opendir(" + dir + ").", errno);
        struct dirent *ptr;
        try {
            while ((ptr = readdir(dirptr)) != NULL) {
                if (strcmp(ptr->d_name, ".") == 0 ||
                        strcmp(ptr->d_name, "..") == 0)
                    continue;
                if (ptr->d_type == DT_DIR)
                    rmdir(pathconcat(dir, ptr->d_name).c_str(), true);
                else {
                    Debug::debug()->trace("Calling remove(%s)", ptr->d_name);
                    int ret = ::remove(pathconcat(dir, ptr->d_name).c_str());
                    if (ret != 0)
                        throw KSystemError("Cannot remove " +
                            string(ptr->d_name) + ".", errno);
                }
            }
        } catch (...) {
            closedir(dirptr);
            throw;
        }
        closedir(dirptr);
        rmdir(dir, false);
    } else {
        int ret = ::rmdir(dir.c_str());
        if (ret != 0)
            throw KSystemError("Cannot rmdir(" + dir + ").", errno);
    }
}

// -----------------------------------------------------------------------------
unsigned long long FileUtil::freeDiskSize(const std::string &path)
    throw (KError)
{
    Debug::debug()->trace("FileUtil::freeDiskSize(%s)", path.c_str());

    struct statfs mystatfs;
    int ret = statfs(path.c_str(), &mystatfs);
    if (ret != 0)
        throw KSystemError("statfs() on " + path + " failed.", errno);

    return (unsigned long long)mystatfs.f_bfree * mystatfs.f_bsize;
}

// -----------------------------------------------------------------------------
unsigned long long FileUtil::fileSize(const std::string &path)
    throw (KError)
{
    Debug::debug()->trace("FileUtil::fileSize(%s)", path.c_str());

    struct stat mystat;
    int ret = ::stat(path.c_str(), &mystat);
    if (ret != 0) {
        throw KSystemError("stat() on " + path + " failed.", errno);
    }

    return (unsigned long long)mystat.st_size;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

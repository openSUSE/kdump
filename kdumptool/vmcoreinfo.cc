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
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <sys/mman.h>
#include <cstring>

#include <libelf.h>
#include <gelf.h>

#include "global.h"
#include "subcommand.h"
#include "debug.h"
#include "savedump.h"
#include "util.h"
#include "fileutil.h"
#include "transfer.h"
#include "configuration.h"
#include "dataprovider.h"
#include "progress.h"
#include "vmcoreinfo.h"
#include "stringutil.h"

using std::string;
using std::cout;
using std::endl;
using std::min;

#define VMCOREINFO_NOTE_NAME           "VMCOREINFO"
#define VMCOREINFO_NOTE_NAME_BYTES     (sizeof(VMCOREINFO_NOTE_NAME))
#define VMCOREINFO_XEN_NOTE_NAME	   "VMCOREINFO_XEN"
#define VMCOREINFO_XEN_NOTE_NAME_BYTES (sizeof(VMCOREINFO_XEN_NOTE_NAME))

#define ELF_HEADER_MAPSIZE          (100*1024)

//{{{ Vmcoreinfo ---------------------------------------------------------------

// -----------------------------------------------------------------------------
Vmcoreinfo::Vmcoreinfo()
    throw (KError)
    : m_xenVmcoreinfo(false)
{
    // check that there are no version inconsitencies
    if (elf_version(EV_CURRENT) == EV_NONE )
        throw KError("libelf is out of date.");
}

// -----------------------------------------------------------------------------
void Vmcoreinfo::readFromELF(const char *elf_file)
    throw (KError)
{
    Debug::debug()->trace("Vmcoreinfo::readFromELF(%s)", elf_file);

    ByteVector vmcoreinfo = readElfNote(elf_file);

    StringVector lines = Stringutil::splitlines(
        Stringutil::bytes2str(vmcoreinfo));

    for (StringVector::const_iterator it = lines.begin();
            it != lines.end(); ++it) {
        KString line = *it;

        Debug::debug()->trace("Line: %s", line.c_str());

        // skip empty lines
        if (line.trim().size() == 0 || line[0] == '\0') {
            continue;
        }

        string::size_type equal = line.find("=");
        if (equal == string::npos) {
            Debug::debug()->info("VMCOREINFO line contains no '='. "
                "Skipping. (%s)sz=%d", line.c_str(), (line).size() );
            continue;
        }

        string key = line.substr(0, equal);
        string value = line.substr(equal+1);

        m_map[key] = value;
        Debug::debug()->trace("%s=%s", key.c_str(), value.c_str());
    }
}

// -----------------------------------------------------------------------------
ByteVector Vmcoreinfo::readElfNote(const char *file)
    throw (KError)
{
    Elf *elf = NULL;
    int fd = -1;
    GElf_Off offset = 0;
    GElf_Xword size = 0;
    char *buffer = NULL;
    bool isElf64;
    void *map = MAP_FAILED;
    char map_copy[ELF_HEADER_MAPSIZE];

    try {
        // open the file
        fd = open(file, O_RDONLY);
        if (fd < 0)
            throw KSystemError("Vmcoreinfo: Cannot open " +
                string(file) + ".", errno);

        // memory-map by hand because the default implementation maps the
        // whole file which fails for dumps on 32 bit systems because they
        // can be larger then the address space
        map = mmap(NULL, ELF_HEADER_MAPSIZE, PROT_READ, MAP_PRIVATE, fd, 0);
        if (map != MAP_FAILED) {
            elf = elf_memory(reinterpret_cast<char *>(map), ELF_HEADER_MAPSIZE);
            if (!elf)
                throw KError("Vmcoreinfo: elf_begin() failed.");

        } else {
            // currently, mmap() on /proc/vmcore does not work
            // so copy it to memory

            for (size_t already_read = 0; already_read < ELF_HEADER_MAPSIZE; ) {

                size_t to_read = min(size_t(BUFSIZ),
                    size_t(ELF_HEADER_MAPSIZE-already_read));
                size_t currently_read = read(fd, map_copy+already_read, to_read);
                if (currently_read == 0)
                    throw KSystemError("Error when reading from dump.", errno);

                already_read += currently_read;
            }

            elf = elf_memory(map_copy, ELF_HEADER_MAPSIZE);
            if (!elf)
                throw KError("Vmcoreinfo: elf_begin() failed.");
        }

        // check the type
        Elf_Kind ek = elf_kind(elf);
        if (ek != ELF_K_ELF)
            throw KError(string(file) + " is no ELF object.");

        // check elf32 vs. elf64
        int clazz = gelf_getclass(elf);
        if (clazz == ELFCLASSNONE)
            throw KError("Vmcoreinfo: Invalid ELF class.");

        isElf64 = clazz == ELFCLASS64;

        // get the number of program header entries
        size_t n;
        int ret = elf_getphdrnum(elf, &n);
        if (ret < 0)
            throw KELFError("elf_getphdrnum() failed.", elf_errno());

        // iterate over that entries to find offset and size of the
        // notes section
        for (unsigned int i = 0; i < n; i++) {
            GElf_Phdr phdr;

            if (gelf_getphdr(elf, i, &phdr) != &phdr)
                throw KELFError("getphdr() failed.", elf_errno());

            if (phdr.p_type == PT_NOTE) {
                offset = phdr.p_offset;
                size = phdr.p_filesz;
                break;
            }
        }

        if (offset == 0 && size == 0)
            throw KError(string(file) + " contains no PT_NOTE segment.");

        Debug::debug()->dbg("PT_NOTE size: %lld, offset: %lld",
            (unsigned long long)size, (unsigned long long)offset);

        buffer = new char[size];

        // seek to the position
        off_t oret = lseek(fd, offset, SEEK_SET);
        if (oret == (off_t)-1)
            throw KSystemError("Vmcoreinfo: lseek() failed.", errno);

        // read the bytes
        ssize_t bytes_read = read(fd, buffer, size);
        if (bytes_read != ssize_t(size))
            throw KSystemError("Vmcoreinfo: Unable to read " +
                Stringutil::number2string(size) +
                " bytes.", errno);
    } catch (...) {
        if (map != MAP_FAILED)
            munmap(map, ELF_HEADER_MAPSIZE);
        if (elf)
            elf_end(elf);
        if (fd > 0)
            ::close(fd);
        if (buffer)
            delete[] buffer;

        throw;
    }

    if (map != MAP_FAILED)
        munmap(map, ELF_HEADER_MAPSIZE);
    if (elf)
        elf_end(elf);
    if (fd > 0)
        ::close(fd);

    ByteVector ret;
    try {
        ret = readVmcoreinfoFromNotes(buffer, size, isElf64);
    } catch (...) {
        delete[] buffer;
        throw;
    }

    delete[] buffer;

    return ret;
}

// -----------------------------------------------------------------------------
ByteVector Vmcoreinfo::readVmcoreinfoFromNotes(const char *buffer, size_t size,
                                               bool isElf64)
    throw (KError)
{

    Elf64_Nhdr *note64;
    Elf32_Nhdr *note32;

    loff_t offset = 0;

    unsigned long long offset_vmcoreinfo = 0, size_vmcoreinfo = 0;

    while (offset < loff_t(size)) {

        loff_t add_offset;

        if (isElf64) {
            note64 = (Elf64_Nhdr *)(buffer + offset);
            add_offset = sizeof(Elf64_Nhdr);
        } else {
            note32 = (Elf32_Nhdr *)(buffer + offset);
            add_offset = sizeof(Elf32_Nhdr);
        }

        if (!strncmp(VMCOREINFO_XEN_NOTE_NAME, buffer+offset+add_offset,
		        VMCOREINFO_XEN_NOTE_NAME_BYTES)) {
			if (isElf64) {
                offset_vmcoreinfo = offset +
				    (sizeof(*note64) + ((note64->n_namesz + 3) & ~3));
				size_vmcoreinfo = note64->n_descsz;
			} else {
				offset_vmcoreinfo = offset +
                    (sizeof(*note32) + ((note32->n_namesz + 3) & ~3));
				size_vmcoreinfo = note32->n_descsz;
			}
            m_xenVmcoreinfo = true;
        } else if (!strncmp(VMCOREINFO_NOTE_NAME, buffer+offset+add_offset,
                    VMCOREINFO_NOTE_NAME_BYTES)) {
            if (isElf64) {
                offset_vmcoreinfo = offset +
                    (sizeof(*note64) + ((note64->n_namesz + 3) & ~3));
                size_vmcoreinfo = note64->n_descsz;
            } else {
                offset_vmcoreinfo = offset  +
                    (sizeof(*note32) + ((note32->n_namesz + 3) & ~3));
                size_vmcoreinfo = note32->n_descsz;
            }

            break;
        }

        if (isElf64)
            offset += sizeof(Elf64_Nhdr) + ((note64->n_namesz + 3) & ~3)
                      + ((note64->n_descsz + 3) & ~3);
        else
            offset += sizeof(Elf32_Nhdr) + ((note32->n_namesz + 3) & ~3)
                      + ((note32->n_descsz + 3) & ~3);
    }

    if (offset_vmcoreinfo == 0 && size_vmcoreinfo == 0)
        throw KError("VMCOREINFO not found.");

    Debug::debug()->dbg("Found VMCOREINFO, offset: %lld, size: %lld",
        offset_vmcoreinfo, size_vmcoreinfo);

    ByteVector ret;
    ret.insert(ret.end(), buffer + offset_vmcoreinfo,
                          buffer + offset_vmcoreinfo + size_vmcoreinfo);
    return ret;
}

// -----------------------------------------------------------------------------
string Vmcoreinfo::getStringValue(const char *key) const
    throw (KError)
{
    StringStringMap::const_iterator pos = m_map.find(string(key));
    if (pos == m_map.end())
        throw KError("Vmcoreinfo: key " + string(key) + " not found.");

    return pos->second;
}

// -----------------------------------------------------------------------------
int Vmcoreinfo::getIntValue(const char *key) const
    throw (KError)
{
    return Stringutil::string2number(getStringValue(key));
}

// -----------------------------------------------------------------------------
int Vmcoreinfo::getLLongValue(const char *key) const
    throw (KError)
{
    return Stringutil::string2llong(getStringValue(key));
}

// -----------------------------------------------------------------------------
StringList Vmcoreinfo::getKeys() const
    throw ()
{
    StringList ret;
    for (StringStringMap::const_iterator it = m_map.begin();
            it != m_map.end(); ++it)
        ret.push_back(it->first);
    return ret;
}

// -----------------------------------------------------------------------------
bool Vmcoreinfo::isXenVmcoreinfo() const
    throw ()
{
    return m_xenVmcoreinfo;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

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
#include <fstream>
#include <cctype>
#include <sstream>
#include <iostream>
#include <cstring>
#include <memory>
#include <cerrno>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <zlib.h>

#include "stringutil.h"
#include "kconfig.h"
#include "debug.h"
#include "util.h"
#include "identifykernel.h"
#include "kerneltool.h"

using std::string;
using std::ifstream;
using std::stringstream;
using std::ostream;
using std::memset;
using std::auto_ptr;
using std::memmove;

#define MAGIC_START         "IKCFG_ST"
#define MAGIC_END           "IKCFG_ED"
#define MAGIC_LEN           8

//{{{ KconfigValue -------------------------------------------------------------


// -----------------------------------------------------------------------------
KconfigValue::KconfigValue()
    throw ()
    : m_type(T_INVALID)
{}

// -----------------------------------------------------------------------------
KconfigValue KconfigValue::fromString(const string &line, string &name)
    throw (KError)
{
    KconfigValue ret;
    string str;

    str = Stringutil::trim(line, "\n");

    // empty line => T_INVALID
    if (str.size() == 0) {
        return ret;
    }

    // "is not set" or comment
    if (str[0] == '#') {

        // is the string does not contain "is not set" then it's a comment
        if (str.find("is not set") == string::npos) {
            return ret;
        }

        if (!(str[1] == ' ' && isalpha(str[2]))) {
            throw KError("Invalid line: '" + str + "'.");
        }

        // now it is none
        string::size_type beginOfConfig = 2;
        string::size_type endOfConfig = str.find(' ', beginOfConfig);

        if (endOfConfig <= beginOfConfig) {
            throw KError("Invalid line: '" + str + "'.");
        }

        name = str.substr(beginOfConfig, endOfConfig-2);

        ret.m_type = T_TRISTATE;
        ret.m_tristate = OFF;

        return ret;
    }

    // now we look if we have a key value pair
    string::size_type equalSign = str.find('=');
    if (equalSign == string::npos) {
        throw KError("Invalid line: '" + str + "'.");
    }

    name = str.substr(0, equalSign);
    
    // the equal sign cannot be the last character
    if (str.size() <= equalSign+1) {
        throw KError("There must be at least one character after =: '" +
            str + "'.");
    }

    string value = str.substr(equalSign+1);

    if (value.size() == 1 && value[0] == 'y') {
        ret.m_type = T_TRISTATE;
        ret.m_tristate = ON;
        return ret;
    } else if (value.size() == 1 && value[0] == 'm') {
        ret.m_type = T_TRISTATE;
        ret.m_tristate = MODULE;
        return ret;
    }
    
    if (Stringutil::isNumber(value)) {
        ret.m_type = T_INTEGER;
        ret.m_integer = Stringutil::string2number(value);
        return ret;
    } else {
        ret.m_type = T_STRING;
        if (value[0] == '"') {
            value = value.substr(1);
        }
        if (value[value.size()-1] == '"') {
            value = value.substr(0, value.size()-1);
        }
        ret.m_string = value;
        return ret;
    }
}

// -----------------------------------------------------------------------------
enum KconfigValue::Type KconfigValue::getType() const
    throw ()
{
    return m_type;
}

// -----------------------------------------------------------------------------
string KconfigValue::getStringValue() const
    throw ()
{
    return m_string;
}

// -----------------------------------------------------------------------------
int KconfigValue::getIntValue() const
    throw ()
{
    return m_integer;
}

// -----------------------------------------------------------------------------
enum KconfigValue::Tristate KconfigValue::getTristateValue() const
    throw ()
{
    return m_tristate;
}

// -----------------------------------------------------------------------------
string KconfigValue::toString() const
    throw ()
{
    stringstream ss;

    switch (m_type) {
        case T_INTEGER:
            ss << "[int] " << m_integer;
            break;

        case T_INVALID:
            ss << "[invalid]";
            break;

        case T_STRING:
            ss << "[string] " << m_string;
            break;

        case T_TRISTATE:
            ss << "[tristate] ";
            switch (m_tristate) {
                case ON:
                    ss << "y";
                    break;

                case OFF:
                    ss << "n";
                    break;

                case MODULE:
                    ss << "m";
                    break;

                default:
                    ss << "invalid value";
                    break;
            }
            break;

        default:
            ss << "[invalid type]";
            break;
    }

    return ss.str();
}

// -----------------------------------------------------------------------------
ostream& operator<<(ostream& os, const KconfigValue& v)
{
    os << v.toString();
    return os;
}


//}}}
//{{{ Kconfig ------------------------------------------------------------------

// -----------------------------------------------------------------------------
void Kconfig::readFromConfig(const string &configFile)
    throw (KError)
{
    Debug::debug()->trace("Kconfig::readFromConfig(%s)", configFile.c_str());

    gzFile fp;
    char line[BUFSIZ];

    fp = gzopen(configFile.c_str(), "r");
    if (!fp) {
        throw KError(string("Opening '") + configFile + string("' failed."));
    }

    try {
        while (gzgets(fp, line, BUFSIZ) != NULL) {
            string name;
            KconfigValue val = KconfigValue::fromString(line, name);
            if (val.getType() != KconfigValue::T_INVALID) {
                m_configs[name] = val;
            }
        }
    } catch (...) {
        gzclose(fp);
        throw;
    }

    gzclose(fp);
}

// -----------------------------------------------------------------------------
void Kconfig::readFromKernel(const string &kernelImage)
    throw (KError)
{
    Debug::debug()->trace("Kconfig::readFromKernel(%s)", kernelImage.c_str());

    stringstream ss;
    ss << extractKernelConfig(kernelImage);

    string line;
    while (getline(ss, line)) {
        string name;
        KconfigValue val = KconfigValue::fromString(line, name);
        if (val.getType() != KconfigValue::T_INVALID) {
            m_configs[name] = val;
        }
    }
}

// -----------------------------------------------------------------------------
string Kconfig::extractKernelConfig(const string &kernelImage)
    throw (KError)
{
    Debug::debug()->trace("Kconfig::extractKernelConfig(%s)",
        kernelImage.c_str());

    KernelTool::KernelType type = KernelTool::getKernelType(kernelImage);
    switch (type) {
        case KernelTool::KT_ELF:
        case KernelTool::KT_ELF_GZ:
            return extractKernelConfigELF(kernelImage);

        case KernelTool::KT_X86:
            return extractKernelConfigbzImage(kernelImage);

        default:
            throw KError("Invalid kernel image: " + kernelImage);
    }
}

// -----------------------------------------------------------------------------
string Kconfig::extractFromIKconfigBuffer(const char *buffer, size_t buflen)
    throw (KError)
{
    Debug::debug()->trace("Kconfig::extractFromIKconfigBuffer(%s, %d)",
        buffer, buflen);

    ssize_t uncompressed_len = buflen * 20;
    auto_ptr<Bytef> uncompressed(new Bytef[uncompressed_len]);

    z_stream stream;
    stream.next_in = (Bytef *)buffer; 
    stream.avail_in = buflen; 

    stream.next_out = (Bytef *)uncompressed.get(); 
    stream.avail_out = uncompressed_len; 

    stream.zalloc = NULL; 
    stream.zfree = NULL; 
    stream.opaque = NULL; 

    int ret = inflateInit2(&stream, -MAX_WBITS); 
    if (ret != Z_OK) { 
        throw KError("inflateInit2() failed");
    }

    ret = inflate(&stream, Z_FINISH); 
    if (ret != Z_STREAM_END) { 
        throw KError("inflate() failed");
    }

    ret = inflateEnd(&stream);
    if (ret != Z_OK) {
        throw KError("Failed to uncompress the kernel configuration ("
            + Stringutil::number2string(ret) + ").");
    }

    uncompressed_len = stream.total_out;

    stringstream ss;
    uncompressed.get()[uncompressed_len] = '\0';
    return string((const char *)uncompressed.get());
}

// -----------------------------------------------------------------------------
string Kconfig::extractKernelConfigELF(const string &kernelImage)
    throw (KError)
{
    Debug::debug()->trace("Kconfig::extractKernelConfigELF(%s)",
        kernelImage.c_str());

    gzFile fp = gzopen(kernelImage.c_str(), "r");
    if (!fp) {
        throw KError(string("Opening '") + kernelImage + string("' failed."));
    }

    // we overlap the reads, that way searching for the pattern is easier
    char buffer[BUFSIZ];
    memset(buffer, 0, BUFSIZ);

    // search for the begin and end offests first
    off_t fileoffset = 0;
    ssize_t chars_read;
    off_t begin_offset = 0, end_offset = 0;
    while ((chars_read = gzread(fp, buffer+MAGIC_LEN, BUFSIZ-MAGIC_LEN)) > 0) {
        if (begin_offset == 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, MAGIC_START, MAGIC_LEN);
            if (pos > 0) {
                begin_offset = fileoffset + pos - MAGIC_LEN;
            }
        } else if (end_offset == 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, MAGIC_END, MAGIC_LEN);
            if (pos > 0) {
                end_offset = fileoffset + pos - MAGIC_LEN;
            }
        } else {
            break;
        }
        fileoffset += chars_read;
        memmove(buffer, buffer+BUFSIZ-MAGIC_LEN, MAGIC_LEN);
    }

    if (begin_offset < 0 || end_offset < 0) {
        gzclose(fp);
        throw KError("Cannot read configuration from " + kernelImage + ".");
    }

    // we need to read after the MAGIC_START and the gzip header
    begin_offset += MAGIC_LEN + 10;

    // read the configuration to a buffer
    if (gzseek(fp, begin_offset, SEEK_SET) == -1) {
        throw KError("gzseek() failed.");
    }
    ssize_t kernelconfig_len = end_offset - begin_offset;
    auto_ptr<char> kernelconfig(new char[kernelconfig_len]);
    
    chars_read = gzread(fp, kernelconfig.get(), kernelconfig_len);
    gzclose(fp);
    if (chars_read != kernelconfig_len) {
        throw KError("Cannot read IKCONFIG.");
    }

    return extractFromIKconfigBuffer(kernelconfig.get(), kernelconfig_len);
}

// -----------------------------------------------------------------------------
string Kconfig::extractKernelConfigbzImage(const string &kernelImage)
    throw (KError)
{
    Debug::debug()->trace("Kconfig::extractKernelConfigbzImage(%s)",
        kernelImage.c_str());

    // that script helped me a lot
    // http://www.cs.caltech.edu/~weixl/research/fast-mon/scripts/extract-ikconfig

    char searchfor[4] = { 0x1f, 0x8b, 0x08, 0x0 };
    char buffer[BUFSIZ];
    ssize_t chars_read;

    int fd = open(kernelImage.c_str(), O_RDONLY);
    if (fd < 0) {
        throw KSystemError("Cannot open " + kernelImage + ".", errno);
    }

    // because of overlapping
    memset(buffer, 0, BUFSIZ);

    off_t fileoffset = 0;
    off_t begin_offset = 0;
    while ((chars_read = read(fd, buffer+4, BUFSIZ-4)) > 0) {
        ssize_t pos = Util::findBytes(buffer, BUFSIZ, searchfor, 4);
        if (pos > 0) {
            begin_offset = fileoffset + pos - MAGIC_LEN;
            break;
        }
        memmove(buffer, buffer+BUFSIZ-MAGIC_LEN, MAGIC_LEN);
        fileoffset += chars_read;
    }

    if (begin_offset == 0) {
        close(fd);
        throw KError("Magic 0x1f 0x8b 0x08 0x0 not found.");
    }

    begin_offset += 4;

    off_t ret = lseek(fd, begin_offset, SEEK_SET);
    if (ret == (off_t)-1) {
        close(fd);
        throw KSystemError("lseek() failed", errno);
    }

    gzFile fp = gzdopen(fd, "r");
    if (!fp) {
        throw KError("gzdopen() failed");
    }

    // we overlap the reads, that way searching for the pattern is easier
    memset(buffer, 0, BUFSIZ);

    // search for the begin and end offests first
    off_t end_offset = 0;
    begin_offset = 0;
    fileoffset = 0;
    while ((chars_read = gzread(fp, buffer+MAGIC_LEN, BUFSIZ-MAGIC_LEN)) > 0) {
        if (begin_offset == 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, MAGIC_START, MAGIC_LEN);
            if (pos > 0) {
                begin_offset = fileoffset + pos - MAGIC_LEN;
            }
        } else if (end_offset == 0) {
            ssize_t pos = Util::findBytes(buffer, BUFSIZ, MAGIC_END, MAGIC_LEN);
            if (pos > 0) {
                end_offset = fileoffset + pos - MAGIC_LEN;
            }
        } else {
            break;
        }
        fileoffset += chars_read;
        memmove(buffer, buffer+BUFSIZ-MAGIC_LEN, MAGIC_LEN);
    }

    if (begin_offset < 0 || end_offset < 0) {
        gzclose(fp);
        close(fd);
        throw KError("Cannot read configuration from " + kernelImage + ".");
    }

    // we need to read after the MAGIC_START and the gzip header
    begin_offset += MAGIC_LEN + 10;

    // read the configuration to a buffer
    if (gzseek(fp, begin_offset, SEEK_SET) == -1) {
        throw KError("gzseek() failed.");
    }
    ssize_t kernelconfig_len = end_offset - begin_offset;
    auto_ptr<char> kernelconfig(new char[kernelconfig_len]);
    
    chars_read = gzread(fp, kernelconfig.get(), kernelconfig_len);
    gzclose(fp);
    close(fd);
    if (chars_read != kernelconfig_len) {
        throw KError("Cannot read IKCONFIG.");
    }

    return extractFromIKconfigBuffer(kernelconfig.get(), kernelconfig_len);
}

// -----------------------------------------------------------------------------
KconfigValue Kconfig::get(const string &option)
    throw ()
{
    return m_configs[option];
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

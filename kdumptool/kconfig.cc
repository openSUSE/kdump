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
    KString str = line;

    str.trim("\n");

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

    KString value = str.substr(equalSign+1);

    if (value.size() == 1 && value[0] == 'y') {
        ret.m_type = T_TRISTATE;
        ret.m_tristate = ON;
        return ret;
    } else if (value.size() == 1 && value[0] == 'm') {
        ret.m_type = T_TRISTATE;
        ret.m_tristate = MODULE;
        return ret;
    }

    if (value.isNumber()) {
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
void Kconfig::readFromKernel(const KernelTool &kt)
    throw (KError)
{
    Debug::debug()->trace("Kconfig::readFromKernel(%s)", kt.toString().c_str());

    stringstream ss;
    ss << kt.extractKernelConfig();

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
void Kconfig::readFromKernel(const string &kernelImage)
    throw (KError)
{
    Debug::debug()->trace("Kconfig::readFromKernel(%s)", kernelImage.c_str());

    stringstream ss;
    KernelTool kt(kernelImage);
    return readFromKernel(kt);
}

// -----------------------------------------------------------------------------
KconfigValue Kconfig::get(const string &option)
    throw ()
{
    return m_configs[option];
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

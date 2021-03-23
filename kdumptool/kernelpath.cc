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

#include "kernelpath.h"
#include "stringutil.h"
#include "kerneltool.h"
#include "util.h"
#include "debug.h"

//{{{ KernelPath --------------------------------------------------------------

// -----------------------------------------------------------------------------
KernelPath::KernelPath(FilePath const &path)
{
    Debug::debug()->trace("KernelPath::KernelPath(%s)", path.c_str());

    FilePath canonical = path.getCanonicalPath();
    m_directory = canonical.dirName();
    KString kernel = canonical.baseName();

    const StringList imageNames = KernelTool::imageNames(Util::getArch());
    for (auto const pfx : imageNames) {
        if (kernel.startsWith(pfx)) {
            if (kernel.length() == pfx.length())
                break;
            if (kernel[pfx.length()] == '-') {
                m_version.assign(kernel, pfx.length() + 1);
                break;
            }
        }
    }

    Debug::debug()->trace("directory=%s, version=%s",
                          m_directory.c_str(), m_version.c_str());
}

// -----------------------------------------------------------------------------
bool KernelPath::isKdump()
{
    bool ret = m_version.endsWith("kdump");
    Debug::debug()->trace("KernelPath::isKdump(), version=%s: %s",
                          m_version.c_str(), ret ? "true" : "false");
    return ret;
}

// -----------------------------------------------------------------------------
FilePath KernelPath::initrdPath(bool fadump)
{
    FilePath ret(m_directory);
    ret.appendPath("initrd");
    if (!m_version.empty()) {
        ret.push_back('-');
        ret.append(m_version);
    }
    if (!fadump && !isKdump())
        ret.append("-kdump");

    Debug::debug()->trace("KernelPath::initrdPath(%s): %s",
                          fadump ? "true" : "false", ret.c_str());
    return ret;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

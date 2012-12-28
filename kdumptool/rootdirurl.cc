/*
 * (c) 2012, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your opti) any later version.
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
#include <sstream>

#include "debug.h"
#include "rootdirurl.h"
#include "fileutil.h"

using std::vector;
using std::copy;

//{{{ RootDirURL ---------------------------------------------------------------

// -----------------------------------------------------------------------------
RootDirURL::RootDirURL(const std::string &url, const std::string &rootdir)
    throw (KError)
    : URLParser(url)
{
    if (getProtocol() == PROT_FILE) {
	m_realpath = FileUtil::getCanonicalPath(getPath(), rootdir);
        Debug::debug()->trace("Real path using root dir (%s): %s",
			      rootdir.c_str(), m_realpath.c_str());
    }
}

//}}}

//{{{ RootDirURLVector ---------------------------------------------------------

// -----------------------------------------------------------------------------
RootDirURLVector::RootDirURLVector(const std::string &urls,
				   const std::string &rootdir)
    throw (KError)
{
    std::istringstream iss(urls);
    std::string url;
    while (iss >> url)
	push_back(RootDirURL(url, rootdir));
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

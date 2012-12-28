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
#ifndef ROOTDIRURL_H
#define ROOTDIRURL_H

#include <string>

#include "urlparser.h"

//{{{ RootDirURL ---------------------------------------------------------------

/**
 * Add root dir support to URLParser
 */
class RootDirURL : public URLParser {

    public:

        /**
         * Creates a new RootDirURL.
         *
         * @param[in] url the URL to parse
	 * @param[in] rootdir the root directory for local files
         * @exception KError if the parsing of that URL fails
         */
        RootDirURL(const std::string &url, const std::string &rootdir)
        throw (KError);

        /**
         * Returns the real path. For local files, this is the canonical
	 * path under the (optional) rootdir prefix.
	 * For non-local files, it is identical to getPath().
         *
         * @return the path
         */
        std::string getRealPath() const
	throw ()
	{
		return m_realpath.empty() ? getPath() : m_realpath;
	}

    private:
	std::string m_realpath;
};

//}}}

//{{{ RootDirURLVector ---------------------------------------------------------

/**
 * Parse a list of URLs.
 */
class RootDirURLVector: public std::vector<RootDirURL> {

    public:

        /**
         * Creates a new RootDirURLVector.
         *
         * @param[in] urls space-separated list of URLs to parse
         * @param[in] rootdir root directory for local files
         * @exception KError if any URL from the list cannot be parsed
         */
        RootDirURLVector(const std::string &urls, const std::string &rootdir)
        throw (KError);

};

//}}}

#endif /* ROOTDIRURL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

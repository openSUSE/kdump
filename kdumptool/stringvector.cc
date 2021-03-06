/*
 * (c) 2021, Petr Tesarik <ptesarik@suse.com>, SUSE Software Solutions Germany, GmbH
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

#include "stringvector.h"

using std::string;

//{{{ StringVector -------------------------------------------------------------

// -----------------------------------------------------------------------------
string StringVector::join(char joiner) const
{
    string result;
    const_iterator it = begin();

    if (it != end()) {
        result = *it;
        while (++it != end()) {
            result += joiner;
            result += *it;
        }
    }

    return result;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

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

#include <cstring>

#include "charv.h"

using std::string;

//{{{ CharV --------------------------------------------------------------------

//------------------------------------------------------------------------------
char **CharV::data()
{
    // copy string data
    m_unique.clear();
    m_unique.reserve(size());
    for (auto const& it : *this) {
        unique_ptr ptr(new char[it.length() + 1]);
        std::strcpy(ptr.get(), it.c_str());
        m_unique.push_back(std::move(ptr));
    }

    // update the pointer array
    m_array.clear();
    m_array.reserve(size() + 1);
    for (auto const& it : m_unique)
        m_array.push_back(it.get());
    m_array.push_back(nullptr);

    return m_array.data();
}

//}}}

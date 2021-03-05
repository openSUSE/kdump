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
#ifndef CHARV_H
#define CHARV_H

#include <vector>
#include <memory>

#include "global.h"

//{{{ CharV --------------------------------------------------------------------

class CharV : public StringVector
{
        typedef std::unique_ptr<char []> unique_ptr;
        std::vector<unique_ptr> m_unique;
        std::vector<char *> m_array;

    public:
        /**
         * Some boiler-plate constructors.
         */
        CharV()
            : StringVector()
        { }
        CharV(const CharV& x)
            : StringVector(x)
        { }
        CharV(const CharV&& x)
            : StringVector(x)
        { }
        CharV(const StringVector& x)
            : StringVector(x)
        { }

        /**
         * Returns a NULL-terminated C string array.
         *
         * @return an dynamically allocated string, which should be freed
         *         with Util::freev()
         */
        char **data();
};

//}}}

#endif /* CHARV_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

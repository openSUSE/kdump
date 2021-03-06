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
#ifndef STRINGVECTOR_H
#define STRINGVECTOR_H

#include <string>
#include <vector>

//{{{ StringVector -------------------------------------------------------------

class StringVector : public std::vector<std::string>
{
    public:
        /**
         * Standard constructors (see std::vector).
         */
        StringVector()
            : std::vector<std::string>()
        { }
        explicit StringVector(const allocator_type& alloc)
            : std::vector<std::string>(alloc)
        { }
        explicit StringVector(size_type n)
            : std::vector<std::string>(n)
        { }
        explicit StringVector(size_type n, const allocator_type& alloc)
            : std::vector<std::string>(n, alloc)
        { }
        StringVector(size_type n, const value_type& val)
            : std::vector<std::string>(n, val)
        { }
        StringVector(size_type n, const value_type& val,
                     const allocator_type& alloc)
            : std::vector<std::string>(n, val, alloc)
        { }
        template <class InputIterator>
                StringVector(InputIterator first, InputIterator last)
                : std::vector<std::string>(first, last)
        { }
        template <class InputIterator>
                StringVector(InputIterator first, InputIterator last,
                             const allocator_type& alloc)
                : std::vector<std::string>(first, last, alloc)
        { }
        StringVector(const std::vector<std::string>& x)
                : std::vector<std::string>(x)
        { }
        StringVector(const std::vector<std::string>& x,
                     const allocator_type& alloc)
                :  std::vector<std::string>(x, alloc)
        { }
        StringVector(std::vector<std::string>&& x)
                : std::vector<std::string>(x)
        { }
        StringVector(std::vector<std::string>&& x,
                     const allocator_type& alloc)
                :  std::vector<std::string>(x, alloc)
        { }
        StringVector(std::initializer_list<value_type> il)
                :  std::vector<std::string>(il)
        { }
        StringVector(std::initializer_list<value_type> il,
                     const allocator_type& alloc)
                :  std::vector<std::string>(il, alloc)
        { }

        /**
         * Join elements into a single string.
         *
         * @param[in] joiner element separator
         * @return the resulting string
         */
        std::string join(char joiner) const;
};

//}}}

#endif /* STRINGVECTOR_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

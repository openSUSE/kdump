/*
 * (c) 2015, Petr Tesarik <ptesarik@suse.com>, SUSE LINUX GmbH
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
#ifndef ROUTABLE_H
#define ROUTABLE_H

#include <string>

#include "subcommand.h"
#include "global.h"

//{{{ Routable -----------------------------------------------------------------

struct addrinfo;

/**
 * Remote target that can be checked for routability.
 */
class Routable {

    public:
	Routable(const std::string &host)
		: m_host(host), m_ai(NULL)
	{}

	~Routable();

	bool check(int timeout);

        const std::string& prefsrc(void) const
        throw ()
        { return m_prefsrc; }

    protected:
	bool resolve(void)
	throw (KError);

	bool hasRoute(void);

    private:
	int m_nlfd;
	std::string m_host;
        std::string m_prefsrc;
	struct addrinfo *m_ai;
};

//}}}

#endif /* ROUTABLE_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

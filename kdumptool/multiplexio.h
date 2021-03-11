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

#ifndef MULTIPLEXIO_H
#define MULTIPLEXIO_H

#include <vector>

#include <poll.h>

//{{{ MultiplexIO --------------------------------------------------------------

class MultiplexIO {

	std::vector<struct pollfd> m_fds;
	int m_active;

    public:
	/**
	 * Trivial constructor.
	 */
	MultiplexIO(void)
            : m_active(0)
        {}

	/**
	 * Add a file descriptor to monitor.
	 *
	 * @param[in] fd file descriptor
	 * @param[in] events to be watched, see poll(2)
	 * @returns corresponding index in the poll vector
	 */
	int add(int fd, short events);

	/**
	 * Get a reference to a pollfd.
	 */
        struct pollfd const &at(int idx) const
        { return m_fds.at(idx); }

	/**
	 * Remove a monitored file descriptor.
	 *
	 * @param[in] idx index in the poll vector
	 */
	void deactivate(int idx);

	/**
	 * Get the number of active file descriptors.
	 */
	int active() const
	{ return m_active; }

	/**
	 * Wait for a monitored event.
	 *
	 * @param[in] timeout max time to wait (in milliseconds)
	 * @returns the number of events
	 */
	int monitor(int timeout = -1);
};

//}}}

#endif /* MULTIPLEXIO_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

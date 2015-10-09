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

#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "global.h"
#include "routable.h"
#include "debug.h"

//{{{ NetLink ------------------------------------------------------------------

#define NETLINK_DEF_RECV_MAX	1024

class NetLink {
    public:
	NetLink(unsigned subscribe = 0U,
		size_t recv_max = NETLINK_DEF_RECV_MAX);

	~NetLink();

	int checkRoute(const struct addrinfo *ai);

	int waitRouteChange(void);

    protected:
	class RecvCheck {
	    public:
		virtual bool check(const struct sockaddr_nl *nladdr,
				   const struct nlmsghdr *nh) const = 0;
	};
	class RouteRecvCheck : public RecvCheck {
	    public:
		virtual bool check(const struct sockaddr_nl *nladdr,
				   const struct nlmsghdr *nh) const;
	};
	class ReplyRecvCheck : public RecvCheck {
	    public:
		ReplyRecvCheck(unsigned peer, unsigned pid, unsigned seq)
		    : m_peer(peer), m_pid(pid), m_seq(seq)
		{}

		virtual bool check(const struct sockaddr_nl *nladdr,
				   const struct nlmsghdr *nh) const;
	    private:
		unsigned m_peer, m_pid, m_seq;
	};

	int receive(const RecvCheck &rc);

	int talk(struct nlmsghdr *req, unsigned peer, unsigned groups);

	struct nlmsghdr *message(void) const
	throw ()
	{ return m_message; }

    private:
	int m_fd;
	struct sockaddr_nl m_local;
	static unsigned m_seq;

	size_t m_buflen;
	unsigned char *m_buffer;
	struct nlmsghdr *m_message;
};

unsigned NetLink::m_seq;

// -----------------------------------------------------------------------------
NetLink::NetLink(unsigned subscribe, size_t recv_max)
    : m_buflen(recv_max)
{
    struct sockaddr_nl sa;
    socklen_t salen;

    m_buffer = new unsigned char[m_buflen];
    m_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (m_fd < 0)
	throw KSystemError("Cannot create netlink", errno);

    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_groups = subscribe;
    if (bind(m_fd, (struct sockaddr *) &sa, sizeof sa) != 0)
	throw KSystemError("Cannot bind netlink", errno);

    salen = sizeof m_local;
    if (getsockname(m_fd, (struct sockaddr*)&m_local, &salen) < 0)
	throw KSystemError("Cannot get local netlink address", errno);
    if (salen != sizeof m_local || m_local.nl_family != AF_NETLINK)
	throw KError("Invalid local netlink address");
}

// -----------------------------------------------------------------------------
NetLink::~NetLink()
{
    if (m_fd >= 0)
	close(m_fd);

    delete[] m_buffer;
}

// -----------------------------------------------------------------------------
bool NetLink::RouteRecvCheck::check(const struct sockaddr_nl *nladdr,
				    const struct nlmsghdr *nh) const
{
    Debug::debug()->trace("RouteRecvCheck::check(%u)",
			  (unsigned)nh->nlmsg_type);

    if (nh->nlmsg_type != RTM_NEWROUTE &&
	nh->nlmsg_type != RTM_DELROUTE)
	return false;

    size_t rtlen = NLMSG_PAYLOAD(nh, 0);
    if (rtlen < sizeof(struct rtmsg))
	throw KError("Netlink rtmsg truncated");

    struct rtmsg *rt = (struct rtmsg*)NLMSG_DATA(nh);
    if (rt->rtm_flags & RTM_F_CLONED)
	return false;

    return true;
}

// -----------------------------------------------------------------------------
bool NetLink::ReplyRecvCheck::check(const struct sockaddr_nl *nladdr,
				    const struct nlmsghdr *nh) const
{
    return (nladdr->nl_pid == m_peer &&
	    nh->nlmsg_pid == m_pid &&
	    nh->nlmsg_seq == m_seq);
}

// -----------------------------------------------------------------------------
int NetLink::receive(const RecvCheck &rc)
{
    struct sockaddr_nl nladdr;
    struct iovec iov;
    struct msghdr msg;

    m_message = NULL;

    msg.msg_name = &nladdr;
    msg.msg_namelen = sizeof nladdr;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    while (1) {
	struct nlmsghdr *nh;
	ssize_t len;

	iov.iov_base = m_buffer;
	iov.iov_len = m_buflen;
	len = recvmsg(m_fd, &msg, 0);
	if (len < 0)
	    throw KSystemError("Cannot receive netlink message", errno);
	if (!len)
	    throw KError("EOF on netlink receive");
	if (msg.msg_namelen != sizeof nladdr)
	    throw KError("Invalid netlink sender address length");

	for (nh = (struct nlmsghdr*)m_buffer; NLMSG_OK(nh, len);
	     nh = NLMSG_NEXT(nh, len)) {
	    if (rc.check(&nladdr, nh)) {
		m_message = nh;

		if (nh->nlmsg_type != NLMSG_ERROR)
		    return 0;

		size_t datalen = NLMSG_PAYLOAD(nh, 0);
		if (datalen < sizeof(struct nlmsgerr))
		    throw KError("Netlink ERROR reply truncated");

		struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(nh);
		return err->error;
	    }
	}

	if (msg.msg_flags & MSG_TRUNC)
	    throw KError("Netlink message truncated");

	if (len)
	    throw KError("Invalid netlink message length");
    }
}

// -----------------------------------------------------------------------------
int NetLink::waitRouteChange(void)
{
    Debug::debug()->trace("waitRouteChange()");

    RouteRecvCheck rc;
    return receive(rc);
}

// -----------------------------------------------------------------------------
int NetLink::talk(struct nlmsghdr *req, unsigned peer, unsigned groups)
{
    struct sockaddr_nl nladdr;
    struct iovec iov;
    struct msghdr msg;
    unsigned seq;
    ssize_t len;

    memset(&nladdr, 0, sizeof nladdr);
    nladdr.nl_family = AF_NETLINK;
    nladdr.nl_pid = peer;
    nladdr.nl_groups = groups;

    memset(&iov, 0, sizeof iov);
    iov.iov_base = (void*)req;
    iov.iov_len = req->nlmsg_len;

    memset(&msg, 0, sizeof msg);
    msg.msg_name = &nladdr;
    msg.msg_namelen = sizeof nladdr;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    req->nlmsg_seq = seq = ++m_seq;

    len = sendmsg(m_fd, &msg, 0);
    if (len < 0)
	throw KSystemError("Cannot send netlink message", errno);

    ReplyRecvCheck rc(peer, m_local.nl_pid, seq);
    return receive(rc);
}

// -----------------------------------------------------------------------------
int NetLink::checkRoute(const struct addrinfo *ai)
{
    size_t addrlen;
    struct {
	struct nlmsghdr  nh;
	struct rtmsg     rt;
	struct rtattr    rta;
	union {
	    struct in_addr in_addr;
	    struct in6_addr in6_addr;
	} u;
    } req;
    int res;

    if (Debug::debug()->isDebugEnabled()) {
	char ip[INET6_ADDRSTRLEN];
	getnameinfo(ai->ai_addr, ai->ai_addrlen, ip, sizeof ip,
		    NULL, 0, NI_NUMERICHOST);
	Debug::debug()->trace("checkRoute(%s)", ip);
    }

    memset(&req, 0, sizeof req);

    if (ai->ai_family == AF_INET) {
	const struct sockaddr_in *sin =
	    (struct sockaddr_in *)ai->ai_addr;

	req.u.in_addr = sin->sin_addr;
	addrlen = sizeof(struct in_addr);
    } else {
	const struct sockaddr_in6 *sin6 =
	    (struct sockaddr_in6 *)ai->ai_addr;

	req.u.in6_addr = sin6->sin6_addr;
	addrlen = sizeof(struct in6_addr);
    }

    req.rta.rta_type = RTA_DST;
    req.rta.rta_len = RTA_LENGTH(addrlen);
    req.rt.rtm_family = ai->ai_family;
    req.rt.rtm_dst_len = addrlen * 8;
    req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg) + req.rta.rta_len);
    req.nh.nlmsg_type = RTM_GETROUTE;
    req.nh.nlmsg_flags = NLM_F_REQUEST;

    if ( (res = talk(&req.nh, 0, 0)) < 0)
	return res;

    struct nlmsghdr *nh = message();
    if (nh->nlmsg_type != RTM_NEWROUTE)
	throw KError("Netlink RTM_GETROUTE reply mismatch");

    size_t rtlen = NLMSG_PAYLOAD(nh, 0);
    if (rtlen < sizeof(struct rtmsg))
	throw KError("Netlink rtmsg truncated");

    struct rtmsg *rt = (struct rtmsg*)NLMSG_DATA(nh);
    switch (rt->rtm_type) {
    case RTN_UNSPEC:
    case RTN_UNICAST:
    case RTN_LOCAL:
    case RTN_BROADCAST:
    case RTN_ANYCAST:
    case RTN_MULTICAST:
	return 0;
    case RTN_UNREACHABLE:
	return -EHOSTUNREACH;
    case RTN_BLACKHOLE:
	return -EINVAL;
    case RTN_PROHIBIT:
	return -EACCES;
    case RTN_THROW:
	return -EAGAIN;
    default:
	return -EINVAL;
    }
}

//}}}

//{{{ Routable -----------------------------------------------------------------

// -----------------------------------------------------------------------------
Routable::~Routable()
{
    if (m_ai)
	freeaddrinfo(m_ai);
}

// -----------------------------------------------------------------------------
bool Routable::hasRoute(void)
{
    NetLink nl;
    struct addrinfo *p;

    Debug::debug()->trace("hasRoute(%s)", m_host.c_str());

    for (p = m_ai; p; p = p->ai_next) {
	if (nl.checkRoute(p) == 0)
	    return true;
    }

    return false;
}

// -----------------------------------------------------------------------------
bool Routable::resolve(void)
    throw (KError)
{
    struct addrinfo hints;
    int res;

    Debug::debug()->trace("resolve(%s)", m_host.c_str());

    if (m_ai)
	freeaddrinfo(m_ai);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_RAW;
    do {
	res = getaddrinfo(m_host.c_str(), NULL, &hints, &m_ai);
    } while (res == EAI_AGAIN);

    if (res == 0)
	return true;

    if (res == EAI_SYSTEM)
	throw KSystemError("Name resolution failed", errno);

    if (res != EAI_NONAME && res != EAI_FAIL && res != EAI_NODATA)
	throw KGaiError("Name resolution failed", res);

    return false;
}

// -----------------------------------------------------------------------------
bool Routable::check(void)
{
    NetLink nl(RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE);

    while (!resolve())
	if (nl.waitRouteChange() != 0)
	    return false;

    while (!hasRoute())
	if (nl.waitRouteChange() != 0)
	    return false;

    return true;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

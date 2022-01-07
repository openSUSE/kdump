/*
 * Copyright (c) 2021 SUSE LLC
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
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 */

#include <stddef.h>
#include <stdio.h>
#include <elf.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>

#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/taskstats.h>

/* How long to wait upon receipt of SIGTERM */
#define SIGTERM_WAIT    5

/* There seems to be no canonical constant for this... */
#define CTRL_GENL_VERSION	0x2

#define RECV_QLEN	16

#define LOG_CONSOLE	"/dev/trackrss"

/* Default log device: ttyS1 */
#define LOG_DEV		makedev(4, 65)

#define SYSTEMD_PATH	"/usr/lib/systemd/systemd"

#define SYSFS		"sysfs"
#define SYSFS_DIR	"/sys"

#define PROCFS		"proc"
#define PROCFS_DIR	"/proc"

#define SYSFS_CPU_POSSIBLE	"/sys/devices/system/cpu/possible"
#define PROCFS_MEMINFO		"/proc/meminfo"

#define MAX_MEMINFO_LINES	100

#define KALLSYMS	"/proc/kallsyms"
#define KCORE		"/proc/kcore"

#define offsetend(type, field) \
	(offsetof(type, field) + sizeof(((type*)0)->field))

#define nlmsg_aligned	__attribute__((aligned(NLMSG_ALIGN(1))))

struct header {
	struct nlmsghdr nlh;
	nlmsg_aligned struct genlmsghdr genlh;
	nlmsg_aligned char payload[];
};

struct connection;

struct recvdata {
	struct connection *conn;

	struct header hdr;
	void *data;
	ssize_t nbytes;
	int err;

	struct timespec stamp;
};

struct connection {
	int fd;

	void *buf;
	size_t bufalloc;

	struct sockaddr_nl addr;
	struct header sendhdr;

	unsigned qhead;
	unsigned qtail;
	int qoverflow;
	struct recvdata *queue[RECV_QLEN+1];
	struct recvdata queuedata[RECV_QLEN+1];

	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_t recv_thread;
};

typedef int attr_callback_t(void *data, struct nlattr *nla, void *payload);

static inline unsigned next_qslot(unsigned idx)
{
	return (idx + 1) % RECV_QLEN;
}

static int conn_init(struct connection *conn)
{
	struct sockaddr_nl sa;
	socklen_t salen;
	unsigned i;

	memset(conn, 0, sizeof(*conn));

	pthread_mutex_init(&conn->mutex, NULL);
	pthread_cond_init(&conn->cond, NULL);

	conn->bufalloc = sysconf(_SC_PAGESIZE);
	conn->buf = malloc(conn->bufalloc * (RECV_QLEN + 1));
	if (!conn->buf) {
		perror("Cannot allocate receive buffer");
		return 1;
	}
	for (i = 0; i <= RECV_QLEN; ++i) {
		struct recvdata *recvdata = &conn->queuedata[i];
		recvdata->conn = conn;
		recvdata->data = conn->buf + i * conn->bufalloc;
		conn->queue[i] = recvdata;
	}

	conn->fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_GENERIC);
	if (conn->fd < 0) {
		perror("Cannot create netlink socket");
		return 1;
	}

	conn->addr.nl_family = AF_NETLINK;
	if (bind(conn->fd, (const struct sockaddr *) &conn->addr,
		 sizeof(conn->addr)) < 0) {
		perror("Cannot bind netlink");
		return 1;
	}

	salen = sizeof(sa);
	if (getsockname(conn->fd, (struct sockaddr*)&sa, &salen) < 0) {
		perror("Cannot get local netlink address");
		return 1;
	}
	if (salen != sizeof(sa) || sa.nl_family != AF_NETLINK) {
		fputs("Invalid local netlink address\n", stderr);
		return 1;
	}
	conn->sendhdr.nlh.nlmsg_pid = sa.nl_pid;

	return 0;
}

static void conn_cleanup(struct connection *conn)
{
	if (conn->fd >= 0)
		close(conn->fd);
	if (conn->buf)
		free(conn->buf);
}

static int conn_send(struct connection *conn, int cmd,
		      void *data, size_t datalen)
{
	struct iovec iov[2];
	struct msghdr msg;
	ssize_t len;

	conn->sendhdr.nlh.nlmsg_len = sizeof(conn->sendhdr) + datalen;
	conn->sendhdr.nlh.nlmsg_flags = NLM_F_REQUEST;
	++conn->sendhdr.nlh.nlmsg_seq;
	conn->sendhdr.genlh.cmd = cmd;

	iov[0].iov_base = (void*)&conn->sendhdr;
	iov[0].iov_len = sizeof(conn->sendhdr);
	iov[1].iov_base = data;
	iov[1].iov_len = datalen;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &conn->addr;
	msg.msg_namelen = sizeof(conn->addr);
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;

	len = sendmsg(conn->fd, &msg, 0);
	if (len < 0) {
		perror("Cannot send netlink message");
		return 1;
	}

	return 0;
}

static void conn_recv(struct connection *conn)
{
	struct iovec iov[2];
	struct msghdr msg;
	struct recvdata *data = conn->queue[RECV_QLEN];
	unsigned qhead;

	iov[0].iov_base = (void*)&data->hdr;
	iov[0].iov_len = sizeof(data->hdr);
	iov[1].iov_base = data->data;
	iov[1].iov_len = conn->bufalloc;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;

	data->nbytes = recvmsg(conn->fd, &msg, 0);
	data->err = errno;
	clock_gettime(CLOCK_MONOTONIC, &data->stamp);

	pthread_mutex_lock(&conn->mutex);
	qhead = conn->qhead;
	conn->qhead = next_qslot(conn->qhead);
	if (conn->qhead != conn->qtail) {
		conn->queue[RECV_QLEN] = conn->queue[qhead];
		conn->queue[qhead] = data;
	} else {
		conn->qhead = qhead;
		conn->qoverflow = 1;
	}
	pthread_cond_signal(&conn->cond);
	pthread_mutex_unlock(&conn->mutex);
}

void *recv_thread_fn(void *arg)
{
	struct connection *conn = arg;
	struct sched_param param;

	param.sched_priority = 99;
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

	for (;;)
		conn_recv(conn);

	return NULL;
}

static int conn_start_recv(struct connection *conn)
{
	if (pthread_create(&conn->recv_thread, NULL, recv_thread_fn, conn)) {
		perror("pthread_create");
		return 1;
	}

	return 0;
}

static int conn_recv_check(struct connection *conn, struct recvdata *data,
			   const char *id, unsigned cmd)
{
	if (data->nbytes < 0) {
		fprintf(stderr, "Cannot receive netlink message: %s\n",
			strerror(data->err));
		return 1;
	}
	if (!NLMSG_OK(&data->hdr.nlh, data->nbytes)) {
		fprintf(stderr, "Netlink message too short: %zd bytes\n",
			data->nbytes);
		return 1;
	}
	if (data->hdr.nlh.nlmsg_type == NLMSG_ERROR) {
		fprintf(stderr, "Netlink error %d\n",
			((struct nlmsgerr*)data->data)->error);
		return 1;
	}
	if (data->hdr.nlh.nlmsg_type != conn->sendhdr.nlh.nlmsg_type) {
		fprintf(stderr, "Unexpected response family %u\n",
			(unsigned)data->hdr.nlh.nlmsg_type);
		return 1;
	}
	if (data->hdr.genlh.cmd != cmd) {
		fprintf(stderr, "Unexpected %s response command: 0x%x\n",
			id, (unsigned)data->hdr.genlh.cmd);
		return 1;
	}

	return 0;
}

static int parse_attrs(void *buffer, size_t buflen,
		       attr_callback_t *cb, void *cbdata)
{
	int ret;

	while (buflen > sizeof(struct nlattr)) {
		struct nlattr *nla = buffer;
		if (nla->nla_len < sizeof(struct nlattr) ||
		    nla->nla_len > buflen) {
			fprintf(stderr, "Wrong attribute length: %u\n",
				(unsigned)nla->nla_len);
			return 1;
		}

		ret = cb(cbdata, nla, buffer + NLA_HDRLEN);
		if (ret)
			return ret;

		buffer += NLA_ALIGN(nla->nla_len);
		buflen -= NLA_ALIGN(nla->nla_len);
	}

	return 0;
}

static int recv_parse_attrs(struct recvdata *data, attr_callback_t *cb)
{
	size_t attrlen = data->hdr.nlh.nlmsg_len - sizeof(struct header);
	return parse_attrs(data->data, attrlen, cb, data);
}

static inline void conn_set_family_id(struct connection *conn,
				      unsigned family, unsigned version)
{
	conn->sendhdr.nlh.nlmsg_type = family;
	conn->sendhdr.genlh.version = version;
}


static int set_family_cb(void *data, struct nlattr *nla, void *payload)
{
	struct recvdata *recv = data;
	if (nla->nla_type == CTRL_ATTR_FAMILY_ID)
		recv->conn->sendhdr.nlh.nlmsg_type = *(__u16*)payload;
	return 0;
}

static int conn_set_family_name(struct connection *conn,
				const char *name, unsigned version)
{
	size_t namelen = strlen(name) + 1;
	size_t datalen = NLA_HDRLEN + NLA_ALIGN(namelen);
	char data[datalen];
	struct nlattr *nla = (struct nlattr*)data;
	struct recvdata *recvdata;

	conn_set_family_id(conn, GENL_ID_CTRL, CTRL_GENL_VERSION);

	nla->nla_len = NLA_HDRLEN + namelen;
	nla->nla_type = CTRL_ATTR_FAMILY_NAME;
	strcpy(data + NLA_HDRLEN, name);
	if (conn_send(conn, CTRL_CMD_GETFAMILY, data, datalen))
		return 1;

	conn_recv(conn);
	recvdata = conn->queue[conn->qtail];
	conn->qtail = next_qslot(conn->qtail);
	if (conn_recv_check(conn, recvdata, "CTRL", CTRL_CMD_NEWFAMILY))
		return 1;

	conn->sendhdr.genlh.version = version;
	return recv_parse_attrs(recvdata, set_family_cb);
}

static int get_possible_cpus(char **mask)
{
	size_t masklen = 0;
	FILE *f;
	int ret = 0;

	f = fopen(SYSFS_CPU_POSSIBLE, "r");
	if (!f || getline(mask, &masklen, f) < 0) {
		perror(SYSFS_CPU_POSSIBLE);
		ret = 1;
	}

	if (f)
		fclose(f);
	return ret;
}

static int register_cpumask(struct connection *conn, const char *mask)
{
	size_t masklen = strlen(mask) + 1;
	size_t datalen = NLA_HDRLEN + NLA_ALIGN(masklen);
	char data[datalen];
	struct nlattr *nla = (struct nlattr*)data;

	nla->nla_len = NLA_HDRLEN + masklen;
	nla->nla_type = TASKSTATS_CMD_ATTR_REGISTER_CPUMASK;
	strcpy(data + NLA_HDRLEN, mask);
	return conn_send(conn, TASKSTATS_CMD_GET, data, datalen);
}

static int taskstats_aggr_cb(void *data, struct nlattr *nla, void *payload)
{
	struct recvdata *recvdata = data;

	if (nla->nla_type != TASKSTATS_TYPE_STATS)
		return 0;

	struct taskstats *ts = payload;

	/* Check that the version field is available. */
	if (nla->nla_len < offsetend(struct taskstats, version)) {
		fprintf(stderr, "Stats attribute too short: %u\n",
			(unsigned) nla->nla_len);
		return 1;
	}

	/* Version 4 changed field alignment, breaking ABI compatibility. */
	if (ts->version < 4) {
		fprintf(stderr, "%s version too old: %u\n",
			TASKSTATS_GENL_NAME, (unsigned) ts->version);
		return 1;
	}

	/* Sanity check before we access uninitialized data. */
	if (nla->nla_len < offsetend(struct taskstats, hiwater_rss)) {
		fprintf(stderr, "Invalid attribute length for v%u: %u\n",
			(unsigned) ts->version, (unsigned) nla->nla_len);
		return 1;
	}

	printf("taskstats:%ld%06ld %lld %lld %s[%d]\n",
	       (long) recvdata->stamp.tv_sec,
	       (long) recvdata->stamp.tv_nsec / 1000,
	       (unsigned long long) ts->ac_etime,
	       (unsigned long long) ts->hiwater_rss,
	       ts->ac_comm,
	       ts->ac_pid);

	return 0;
}

static int taskstats_cb(void *data, struct nlattr *nla, void *payload)
{
	if (nla->nla_type == TASKSTATS_TYPE_AGGR_PID)
		return parse_attrs(payload, nla->nla_len,
				   taskstats_aggr_cb, data);
	return 0;
}

static int get_taskstats(struct connection *conn)
{
	struct recvdata *recvdata;
	int overflow;

	pthread_mutex_lock(&conn->mutex);
	if (conn->qtail == conn->qhead)
		pthread_cond_wait(&conn->cond, &conn->mutex);

	while (conn->qtail != conn->qhead) {
		recvdata = conn->queue[conn->qtail];
		conn->qtail = next_qslot(conn->qtail);
		overflow = conn->qoverflow;
		conn->qoverflow = 0;
		pthread_mutex_unlock(&conn->mutex);

		if (overflow)
			fputs("WARNING: Netlink ringbuffer overflow!\n",
			      stderr);

		if (conn_recv_check(conn, recvdata,
				    TASKSTATS_GENL_NAME, TASKSTATS_CMD_NEW))
			return 1;

		if (recv_parse_attrs(recvdata, taskstats_cb))
			return 1;

		pthread_mutex_lock(&conn->mutex);
	}

	pthread_mutex_unlock(&conn->mutex);

	return 0;
}

static int mount_failure(const char *what)
{
	perror(what);
	return 1;
}

static int init_mounts(void)
{
	if (mount(SYSFS, SYSFS_DIR, SYSFS,
		  MS_NOSUID|MS_NOEXEC|MS_NODEV, NULL))
		return mount_failure(SYSFS);

	if (mount(PROCFS, PROCFS_DIR, PROCFS,
		  MS_NOSUID|MS_NOEXEC|MS_NODEV, NULL))
		return mount_failure(PROCFS);

	return 0;
}

struct syminfo;
typedef int readfn(struct syminfo *info, void *buf,
		   unsigned long addr, size_t len);

struct syminfo {
	FILE *f;
	unsigned phnum;
	union {
		Elf32_Phdr *phdr32;
		Elf64_Phdr *phdr64;
	};
	readfn *read;

	unsigned long datasym;
	unsigned long sizesym;
};

static int get_syms(struct syminfo *info)
{
	static const char name_data[] = "vmcoreinfo_data";
	static const char name_size[] = "vmcoreinfo_size";
	size_t alloc = 0;
	char *line = NULL;
	int have_name = 0;
	int have_size = 0;
	ssize_t rd;

	while ((rd = getline(&line, &alloc, info->f)) > 0) {
		char *p = line + rd - 1;
		while (p != line && *p == '\n')
			*p-- = 0;
		if (p >= line + sizeof(name_data) &&
		    !strcmp(p - sizeof(name_data) + 2, name_data))
			have_name = sscanf(line, "%lx", &info->datasym);
		else if (p >= line + sizeof(name_size) &&
			 !strcmp(p - sizeof(name_size) + 2, name_size))
			have_size = sscanf(line, "%lx", &info->sizesym);
	}
	free(line);

	if (!have_name) {
		fprintf(stderr, "%s symbol not found!\n", name_data);
		return 1;
	}
	if (!have_size) {
		fprintf(stderr, "%s symbol not found!\n", name_size);
		return 1;
	}

	return 0;
}

static int read_common(struct syminfo *info, void *buf,
		       off_t off, size_t len)
{
	if (fseek(info->f, off, SEEK_SET) < 0) {
		perror("Seek to data");
		return 1;
	}

	if (fread(buf, 1, len, info->f) != len) {
		perror("Read data");
		return 1;
	}

	return 0;
}

static int read_elf32(struct syminfo *info, void *buf,
		      unsigned long addr, size_t len)
{
	unsigned i;

	for (i = 0; i < info->phnum; ++i) {
		Elf32_Phdr *phdr = info->phdr32 + i;
		if (phdr->p_vaddr <= addr &&
		    addr + len <= phdr->p_vaddr + phdr->p_filesz)
			return read_common(info, buf, addr - phdr->p_vaddr + phdr->p_offset, len);
	}

	fprintf(stderr, "Address 0x%lx not found!\n", addr);
	return 1;
}

static int read_elf64(struct syminfo *info, void *buf,
		      unsigned long addr, size_t len)
{
	unsigned i;

	for (i = 0; i < info->phnum; ++i) {
		Elf64_Phdr *phdr = info->phdr64 + i;
		if (phdr->p_vaddr <= addr &&
		    addr + len <= phdr->p_vaddr + phdr->p_filesz)
			return read_common(info, buf, addr - phdr->p_vaddr + phdr->p_offset, len);
	}

	fprintf(stderr, "Address 0x%lx not found!\n", addr);
	return 1;
}

static int print_vmcoreinfo_common(struct syminfo *info)
{
	unsigned char *dataptr;
	size_t size;
	int ret;

	if (info->read(info, &dataptr, info->datasym, sizeof(dataptr)))
		return 1;

	if (info->read(info, &size, info->sizesym, sizeof(size)))
		return 1;

	char *data = malloc(size);
	if (!data) {
		perror("Allocate vmcoreinfo data");
		return 1;
	}
	ret = info->read(info, data, (uintptr_t)dataptr, size);
	if (!ret) {
		char *savep;
		char *p = strtok_r(data, "\r\n", &savep);
		while (p) {
			printf("vmcoreinfo:%s\n", p);
			p = strtok_r(NULL, "\r\n", &savep);
		}
	}
	free(data);
	return ret;
}

static int print_vmcoreinfo32(struct syminfo *info)
{
	Elf32_Ehdr ehdr;

	if (fseek(info->f, 0, SEEK_SET) < 0) {
		perror("Seek to ELF header");
		return 1;
	}
	if (fread(&ehdr, sizeof(ehdr), 1, info->f) != 1) {
		perror("ELF header");
		return 1;
	}
	if (ehdr.e_phentsize != sizeof(Elf32_Phdr)) {
		fprintf(stderr, "Invalid phentsize: %zu\n",
			(size_t) ehdr.e_phentsize);
		return 1;
	}
	info->phnum = ehdr.e_phnum;

	if (fseek(info->f, ehdr.e_phoff, SEEK_SET) < 0) {
		perror("Seek to program headers");
		return 1;
	}
	info->phdr32 = malloc(info->phnum * sizeof(Elf32_Phdr));
	if (!info->phdr32) {
		perror("Alloc program headers");
		return 1;
	}
	if (fread(info->phdr32, sizeof(Elf32_Phdr), info->phnum, info->f)
	    != info->phnum) {
		perror("Read program headers");
		return 1;
	}
	info->read = read_elf32;
	return print_vmcoreinfo_common(info);
}

static int print_vmcoreinfo64(struct syminfo *info)
{
	Elf64_Ehdr ehdr;

	if (fseek(info->f, 0, SEEK_SET) < 0) {
		perror("Seek to ELF header");
		return 1;
	}
	if (fread(&ehdr, sizeof(ehdr), 1, info->f) != 1) {
		perror("ELF header");
		return 1;
	}
	if (ehdr.e_phentsize != sizeof(Elf64_Phdr)) {
		fprintf(stderr, "Invalid phentsize: %zu\n",
			(size_t) ehdr.e_phentsize);
		return 1;
	}
	info->phnum = ehdr.e_phnum;

	if (fseek(info->f, ehdr.e_phoff, SEEK_SET) < 0) {
		perror("Seek to program headers");
		return 1;
	}
	info->phdr64 = malloc(info->phnum * sizeof(Elf64_Phdr));
	if (!info->phdr64) {
		perror("Alloc program headers");
		return 1;
	}
	if (fread(info->phdr64, sizeof(Elf64_Phdr), info->phnum, info->f)
	    != info->phnum) {
		perror("Read program headers");
		return 1;
	}
	info->read = read_elf64;
	return print_vmcoreinfo_common(info);
}

static int print_kcore_vmcoreinfo(struct syminfo *info)
{
	unsigned char ident[EI_NIDENT];

	if (fread(ident, sizeof(ident), 1, info->f) != 1) {
		perror("ELF ident");
		return 1;
	}

	if (ident[EI_CLASS] == ELFCLASS32)
		return print_vmcoreinfo32(info);
	else if (ident[EI_CLASS] == ELFCLASS64)
		return print_vmcoreinfo64(info);

	fprintf(stderr, "Unimplemented ELF class: %u\n", (int)ident[EI_CLASS]);
	return 1;
}

static int print_vmcoreinfo(void)
{
	struct syminfo syminfo;
	int ret;

	syminfo.f = fopen(KALLSYMS, "r");
	if (!syminfo.f) {
		perror(KALLSYMS);
		return 1;
	}
	ret = get_syms(&syminfo);
	fclose(syminfo.f);
	if (ret)
		return ret;

	syminfo.f = fopen(KCORE, "r");
	if (!syminfo.f) {
		perror(KCORE);
		return 1;
	}
	ret = print_kcore_vmcoreinfo(&syminfo);
	fclose(syminfo.f);
	return ret;
}

static int get_meminfo(char **info, int max)
{
	FILE *f;

	f = fopen(PROCFS_MEMINFO, "r");
	if (!f) {
		perror(PROCFS_MEMINFO);
		return 1;
	}

	int n = 0;
	for (;;) {
		char *line = NULL;
		size_t alloc = 0;
		errno = 0;
		ssize_t len = getline(&line, &alloc, f);
		if (len < 0) {
			if (errno != 0) {
				perror(PROCFS_MEMINFO);
				free(line);
				n = -1;
			}
			break;
		}

		char *p = line + len - 1;
		while (p > line && (*p == '\r' || *p == '\n'))
			*p-- = '\0';

		if (n < max)
			info[n] = line;
		if (++n == max)
			fprintf(stderr, "WARNING: Too many lines in %s\n",
				PROCFS_MEMINFO);
	}

	fclose(f);
	return n;
}

static void free_meminfo(char **info, int num)
{
	while (num > 0) {
		free(*info);
		++info;
		--num;
	}
}

static void print_meminfo(char **info, int num)
{
	while (num > 0) {
		printf("%s:%s\n", "meminfo", *info);
		++info;
		--num;
	}
}

static int start_systemd(char *argv[])
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return 1;
	} else if (pid > 0) {
		execv(SYSTEMD_PATH, argv);
		perror("execv");
		_exit(1);
	}

	/* Daemonize the child. */

	if(setsid() < 0) {
		perror("setsid");
		return 1;
	}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return 1;
	} else if (pid > 0)
		_exit(0);

	if (mknod(LOG_CONSOLE, S_IFCHR | 0660, LOG_DEV) < 0 &&
	    errno != EEXIST) {
		perror("create console");
		return 1;
	}

	int fd = open(LOG_CONSOLE, O_WRONLY);
	if (fd < 0) {
		perror("open console");
		return 1;
	}
	if (dup2(fd, STDOUT_FILENO) < 0){
		perror("dup2");
		return 1;
	}
	close(fd);

	return 0;
}

static void sigterm_handler(int signo)
{
        sleep(SIGTERM_WAIT);
        exit(0);
}

int main(int argc, char *argv[])
{
	struct connection conn;
	char *cpumask = NULL;
	char *meminfo[MAX_MEMINFO_LINES];
	int infonum;
	int ret;

        signal(SIGTERM, sigterm_handler);

	ret = init_mounts();
	if (ret)
		return ret;

	infonum = get_meminfo(meminfo, MAX_MEMINFO_LINES);
	if (infonum < 0)
		return 1;

	ret = conn_init(&conn);
	if (ret)
		goto done;

	ret = conn_set_family_name(&conn, TASKSTATS_GENL_NAME,
				   TASKSTATS_GENL_VERSION);
	if (!ret)
		ret = get_possible_cpus(&cpumask);
	if (!ret)
		ret = register_cpumask(&conn, cpumask);

	if (cpumask)
		free(cpumask);

	if (start_systemd(argv))
		return 1;

	if (print_vmcoreinfo())
		return 1;

	print_meminfo(meminfo, infonum);
	free_meminfo(meminfo, infonum);

	if (!ret)
		ret = conn_start_recv(&conn);

	while (!ret) {
		ret = get_taskstats(&conn);
	}

 done:
	conn_cleanup(&conn);
	return ret;
}

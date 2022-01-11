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
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <linux/random.h>

/* How long to wait upon receipt of SIGTERM */
#define SIGTERM_WAIT	5

#define LOG_CONSOLE	"/dev/trackrss"

/* Default log device: ttyS1 */
#define LOG_DEV		makedev(4, 65)

/* Random number generator device */
#define RANDOM_DEV	makedev(1, 8)
#define RANDOM_PATH	"/dev/random"

#define SYSTEMD_PATH	"/usr/lib/systemd/systemd"

#define SYSFS		"sysfs"
#define SYSFS_DIR	"/sys"

#define PROCFS		"proc"
#define PROCFS_DIR	"/proc"

#define TRACEFS		"tracefs"
#define TRACEFS_DIR	"/sys/kernel/tracing"

#define PROCFS_MEMINFO		"/proc/meminfo"

#define MAX_MEMINFO_LINES	100

#define TRACE_LINE_LENGTH	256

#define KALLSYMS	"/proc/kallsyms"
#define KCORE		"/proc/kcore"

struct connection {
	FILE *f;
};

static int conn_init(struct connection *conn)
{
	char path[PATH_MAX];

	memset(conn, 0, sizeof(*conn));

	snprintf(path, PATH_MAX, "%s/%s", TRACEFS_DIR, "trace_pipe");
	conn->f = fopen(path, "r");
	if (!conn->f) {
		perror(path);
		return 1;
	}

	return 0;
}

static void conn_cleanup(struct connection *conn)
{
	if (conn->f)
		fclose(conn->f);
}

static int get_trace(struct connection *conn)
{
	char line[TRACE_LINE_LENGTH];

	while (fgets(line, sizeof(line), conn->f))
		printf("trace:%s", line);

	if (errno) {
		perror("Read from trace pipe");
		return 1;
	}

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

	if (mount(TRACEFS, TRACEFS_DIR, TRACEFS,
		  MS_NOSUID|MS_NOEXEC|MS_NODEV, NULL))
		return mount_failure(TRACEFS);

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

static int write_tracefs(const char *subpath, const char *content)
{
	char path[PATH_MAX + 1];
	FILE *f;
	int ret;

	path[PATH_MAX] = 0;
	snprintf(path, PATH_MAX, "%s/%s", TRACEFS_DIR, subpath);
	f = fopen(path, "w");
	if (!f) {
		perror(subpath);
		return 1;
	}
	ret = fputs(content, f) < 0;
	if (ret)
		perror(subpath);
	fclose(f);
	return ret;
}

static int init_tracing(void)
{

	int ret;

	ret = write_tracefs("saved_cmdlines_size", "256");
	if (ret)
		return ret;

	ret = write_tracefs("events/kmem/rss_stat/filter", "member == 1");
	if (ret)
		return ret;

	ret = write_tracefs("events/kmem/rss_stat/enable", "1");
	if (ret)
		return ret;

	ret = write_tracefs("trace_options", "noirq-info");
	if (ret)
		return ret;

	return write_tracefs("tracing_on", "1");
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

static int console_fd = -1;

static int open_console(char *argv[])
{
        static const char opt[] = "trackrss=";
        dev_t dev = LOG_DEV;
        char **arg, **lastarg;
        for (arg = lastarg = &argv[1]; *arg; ++arg) {
                if (strncmp(*arg, opt, sizeof(opt) - 1) == 0) {
                        char *p = *arg + sizeof(opt) - 1;
                        int maj, min;
                        if (sscanf(p, "%d,%d", &maj, &min) != 2) {
                                fprintf(stderr, "Invalid option: %s\n", *arg);
                                return 1;
                        }
                        dev = makedev(maj, min);
                } else
                        *lastarg++ = *arg;
        }
        *lastarg = NULL;

	if (mknod(LOG_CONSOLE, S_IFCHR | 0660, dev) < 0) {
		perror("create console");
		return 1;
	}

	console_fd = open(LOG_CONSOLE, O_WRONLY);
	if (console_fd < 0) {
		perror("open console");
		return 1;
	}

        if (unlink(LOG_CONSOLE) < 0) {
                perror("unlink console");
                return 1;
        }

        return 0;
}

static int init_random(void)
{
	int fd;

	if (mknod(RANDOM_PATH, S_IFCHR | 0666, RANDOM_DEV) < 0) {
		perror("create random");
		return 1;
	}

	fd = open(RANDOM_PATH, O_WRONLY);
	if (fd < 0) {
		perror("open random");
		return 1;
	}

        if (unlink(RANDOM_PATH) < 0) {
                perror("unlink random");
		goto err;
        }

	/* Bogus non-entropy, but who cares... */
	int entcnt = 1024;
	if (ioctl(fd, RNDADDTOENTCNT, &entcnt) < 0) {
		perror("add random");
		goto err;
	}

	close(fd);
	return 0;

err:
	close(fd);
	return 1;
}

static int start_systemd(char *argv[])
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return 1;
	} else if (pid > 0) {
                close(console_fd);
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

	if (dup2(console_fd, STDOUT_FILENO) < 0){
		perror("dup2");
		return 1;
	}
	close(console_fd);

	return 0;
}

static void sigterm_handler(int signo)
{
	alarm(SIGTERM_WAIT);
}

static void sigalrm_handler(int signo)
{
	_exit(0);
}

int main(int argc, char *argv[])
{
	struct connection conn;
	char *meminfo[MAX_MEMINFO_LINES];
	int infonum;
	int ret;

        signal(SIGTERM, sigterm_handler);
	signal(SIGALRM, sigalrm_handler);

        ret = open_console(argv);
        if (ret)
                return ret;

	ret = init_random();
	if (ret)
		return ret;

	ret = init_mounts();
	if (ret)
		return ret;

	infonum = get_meminfo(meminfo, MAX_MEMINFO_LINES);
	if (infonum < 0)
		return 1;

	ret = init_tracing();
	if (ret)
		return ret;

	ret = conn_init(&conn);
	if (ret)
		goto done;

	if (start_systemd(argv))
		return 1;

	if (print_vmcoreinfo())
		return 1;

	print_meminfo(meminfo, infonum);
	free_meminfo(meminfo, infonum);

	while (!ret) {
		ret = get_trace(&conn);
	}

 done:
	conn_cleanup(&conn);
	return ret;
}

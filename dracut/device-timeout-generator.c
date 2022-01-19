#define _GNU_SOURCE
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <syslog.h>
#include <fstab.h>

#include <blkid.h>

#define DEV_KMSG	"/dev/kmsg"
#define PROC_CMDLINE	"/proc/cmdline"
#define ETC_CMDLINE	"/etc/cmdline"
#define ETC_CMDLINE_D	"/etc/cmdline.d"
#define CONF_SUFFIX	".conf"
#define CONF_SUFFIX_LEN	(sizeof(CONF_SUFFIX)-1)

#define KDUMP_DIR	"/kdump/"
#define KDUMP_DIR_LEN	(sizeof(KDUMP_DIR)-1)

#define DEV_PREFIX	"/dev/disk/by-"
#define DEV_PREFIX_LEN	(sizeof(DEV_PREFIX)-1)

#define READ_CHUNK_ADD	1024
#define READ_CHUNK_MIN	256

typedef int parser_fn(const char *key, const char *val);
static parser_fn get_timeout;

static const char program_name[] = "device-timeout-generator";

/* Variables that do not change can be global */
static const char *unitdir;
static size_t unitdirlen;

static unsigned long timeout;

static int
error(const char *fmt, ...)
{
	static int fd = -1;
	char *msg, *kmsg;
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vasprintf(&msg, fmt, ap);
	if (len < 0)
		return len;
	fprintf(stderr, "%s\n", msg);
	va_end(ap);

	if (fd < 0) {
		fd = open(DEV_KMSG, O_WRONLY|O_NOCTTY|O_CLOEXEC);
		if (fd < 0) {
			free(msg);
			return -1;
		}
	}

	len = asprintf(&kmsg, "<%d> %s [%lu]: %s\n",
		       LOG_ERR, program_name, (unsigned long)getpid(), msg);
	free(msg);
	if (len < 0)
		return len;

	len = write(fd, kmsg, len);
	free(kmsg);
	return len;
}

static inline int
isoctal(char c)
{
	return c >= '0' && c <= '7';
}

static char *
skipws(char *s)
{
	while (isspace(*s))
		++s;
	return s;
}

static void
unescape_fstab(char *spec)
{
	char *p, *q;

	p = q = spec;
	while (*p) {
		if (p[0] == '\\' &&
		    isoctal(p[1]) && isoctal(p[2]) && isoctal(p[3])) {
			*q++ = ((p[1] - '0') << 6) |
				((p[2] - '0') << 3) |
				(p[3] - '0');
			p += 3;
		} else
			*q++ = *p++;
	}
}

static void
unquote(char **p, char *endp)
{
	if (*p && **p == '"') {
		++(*p);
		if (endp[-1] == '"')
			endp[-1] = '\0';
	}
}

static char *
get_arg(char *args, char **key, char **val)
{
	int inquote;

	*key = args;
	*val = NULL;
	inquote = 0;
	while (*args && (inquote || !isspace(*args))) {
		if (*args == '"')
			inquote = !inquote;
		else if (*args == '=' && !*val) {
			*args = '\0';
			*val = args + 1;
		}
		++args;
	}
	unquote(key, args);
	unquote(val, args);

	if (*args)
		*args++ = '\0';
	return skipws(args);
}

static int
parse_args(char *args, parser_fn *parse)
{
	char *key, *val, *p;

	p = skipws(args);
	while (*p) {
		p = get_arg(p, &key, &val);
		if (parse(key, val))
			break;
	}
	return 0;
}

static char *
slurp_fd(int fd)
{
	char *buf, *bufp;
	size_t len, remain;
	ssize_t rdlen;

	buf = bufp = NULL;
	len = remain = 0;
	do {
		if (remain < READ_CHUNK_MIN) {
			char *newbuf = realloc(buf, len + READ_CHUNK_ADD + 1);
			if (!newbuf) {
				free(buf);
				return NULL;
			}
			len += READ_CHUNK_ADD;
			remain += READ_CHUNK_ADD;
			bufp = newbuf + (bufp - buf);
			buf = newbuf;
		}

		rdlen = read(fd, bufp, remain);
		remain -= rdlen;
	} while (rdlen > 0);

	if (rdlen < 0) {
		free(buf);
		return NULL;
	}

	buf[len - remain] = '\0';
	return buf;
}

static char *
slurp_file(const char *path)
{
	int fd;
	char *ret;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		error("Cannot open '%s': %s", path, strerror(errno));
		return NULL;
	}

	ret = slurp_fd(fd);
	if (!ret)
		error("Cannot read '%s': %s", path, strerror(errno));
	return ret;
}

static int
parse_file(const char *fname, parser_fn *parse)
{
	char *args;
	int ret;

	args = slurp_file(fname);
	if (!args)
		return -1;

	ret = parse_args(args, parse);
	free(args);
	return ret;
}

static int
parse_dir(const char *dname, parser_fn *parse)
{
	DIR *dir;
	struct dirent *e;

	dir = opendir(dname);
	if (!dir) {
		if (errno != ENOENT)
			error("Cannot open '%s': %s", dname, strerror(errno));
		return -1;
	}

	while ( (e = readdir(dir)) ) {
		size_t len = strlen(e->d_name);
		char *endp = e->d_name + len;
		int fd;
		char *args;

		if (len < CONF_SUFFIX_LEN ||
		    strcmp(endp - CONF_SUFFIX_LEN, CONF_SUFFIX))
			continue;
		fd = openat(dirfd(dir), e->d_name, O_RDONLY);
		if (fd < 0) {
			error("Cannot open '%s': %s",
			      e->d_name, strerror(errno));
			continue;
		}

		args = slurp_fd(fd);
		if (!args) {
			error("Cannot read '%s': %s",
			      e->d_name, strerror(errno));
			continue;
		}

		parse_args(args, parse);
		free(args);
	}
	closedir(dir);
	return 0;
}

static int
parse_cmdline_files(parser_fn *parse)
{
	parse_dir(ETC_CMDLINE_D, parse);

	if (!access(ETC_CMDLINE, R_OK))
		parse_file(ETC_CMDLINE, parse);

	return parse_file(PROC_CMDLINE, parse);
};

static int
get_timeout(const char *key, const char *val)
{
	if (!strcmp(key, "rd.timeout") && val && *val) {
		char *end;
		unsigned long num = strtoul(val, &end, 0);
		if (*end == '\0')
			timeout = num;
		else
			error("Invalid rd.timeout format: %s", val);
	}
	return 0;
}

static char *
evaluate_spec(const char *spec)
{
	char *tag, *value;
	size_t len;
	char *p;
	char *ret, *retp;

	tag = strdup(spec);
	if (!tag)
		return NULL;

	value = strchr(tag, '=');
	if (!value)
		return tag;

	*value++ = '\0';
	if (*value == '"' || *value == '\'') {
		char c = *value++;

		p = strrchr(value, c);
		if (!p) {
			errno = -EINVAL;
			return NULL;
		}
		*p = '\0';
	}

	len = 4 * strlen(value);
	ret = malloc(DEV_PREFIX_LEN + strlen(tag) + 1 + len + 1);
	retp = stpcpy(ret, DEV_PREFIX);
	for (p = tag; *p; ++p)
		*retp++ = tolower(*p);

	*retp++ = '/';
	blkid_encode_string(value, retp, len + 1);

	free(tag);
	return ret;
}

static inline char
hex_digit(unsigned d)
{
	return d < 10 ? d + '0' : d - 10 + 'a';
}

static char *
hex_escape(char *s, char c)
{
	*s++ = '\\';
	*s++ = 'x';
	*s++ = hex_digit((unsigned)c >> 4);
	*s++ = hex_digit((unsigned)c & 0x0f);
	return s;
}

static inline int
safe_unit_char(char c)
{
	return isalnum(c) || c == ':' || c == '_' || c == '.';
}

static char *
path_escape(char *path)
{
	size_t pathlen = strlen(path);
	char *dedup, *ret;
	char *src, *dst;

	ret = malloc(pathlen * 4 + 1);
	if (!ret)
		return ret;

	/* remove duplicate slashes */
	dst = dedup = ret + pathlen * 3;
	for (src = path; *src; ++src) {
		if (src[0] != '/' || (src[1] && src[1] != '/'))
			*dst++ = *src;
	}
	*dst = '\0';

	/* root directory special case */
	if (*dedup == '\0')
		return strcpy(ret, "-");

	dst = ret;
	src = dedup;
	if (*src == '/')
		++src;
	if (*src == '.')
		dst = hex_escape(dst, *src++);

	while (*src) {
                if (*src == '/')
                        *dst++ = '-';
                else if (!safe_unit_char(*src))
                        dst = hex_escape(dst, *src);
                else
                        *dst++ = *src;
		++src;
        }
	*dst = '\0';

        return ret;
}

static int
write_conf(const char *path)
{
	FILE *f;

	f = fopen(path, "w");
	if (!f) {
		error("Cannot open file '%s': %s", path, strerror(errno));
		return -1;
	}

	if (fprintf(f, "[Unit]\nJobTimeoutSec=%lu\n", timeout) < 0) {
		error("Cannot write to '%s': %s", path, strerror(errno));
		return -1;
	}

	if (fclose(f)) {
		error("Cannot close '%s': %s", path, strerror(errno));
		return -1;
	}

	return 0;
}

static int
create_conf(const char *spec)
{
	char *devname, *unitname, *confpath, *p;
	int ret;

	devname = evaluate_spec(spec);
	if (!devname) {
		error("Cannot convert '%s' to device name: %s",
		      spec, strerror(errno));
		return -1;
	}

	unitname = path_escape(devname);
	if (!unitname) {
		error("Cannot convert '%s' to systemd unit name: %s",
		      devname, strerror(errno));
		free(devname);
		return -1;
	}
	free(devname);

	confpath = malloc(unitdirlen + 1 + strlen(unitname) +
			  sizeof(".device.d/timeout.conf"));
	if (!confpath) {
		error("Cannot allocate path for '%s': %s",
		      unitname, strerror(errno));
		free(unitname);
		return -1;
	}
	p = stpcpy(confpath, unitdir);
	*p++ = '/';
	p = stpcpy(p, unitname);
	p = stpcpy(p, ".device.d");
	free(unitname);

	if (mkdir(confpath, S_IRWXU | S_IRWXG | S_IRWXO) && errno != EEXIST) {
		error("Cannot create directory '%s': %s",
		      confpath, strerror(errno));
		free(confpath);
		return -1;
	}

	p = stpcpy(p, "/timeout.conf");
	ret = write_conf(confpath);
	free(confpath);
	return ret;
}

int
main(int argc, char **argv)
{
	struct fstab *fs;

	umask(S_IWGRP | S_IWOTH);

	if (argc != 4) {
		error("Wrong number of arguments: %d", argc);
		return EXIT_FAILURE;
	}
	unitdir = argv[1];
	unitdirlen = strlen(argv[1]);

	parse_cmdline_files(get_timeout);

	while ((fs = getfsent()) != NULL) {
		if (strncmp(fs->fs_file, KDUMP_DIR, KDUMP_DIR_LEN))
			continue;

		unescape_fstab(fs->fs_spec);
		create_conf(fs->fs_spec);
	}

	return EXIT_SUCCESS;
}

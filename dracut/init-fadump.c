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

#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <linux/magic.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>

#define PROC		"proc"
#define PROC_DIR	"/proc"

#define FADUMP_DIR	"/fadumproot"

#define PROC_VMCORE	"/proc/vmcore"

#define DRACUT_INIT	"/init.dracut"

#define KERNEL_CMDLINE  (PROC_DIR "/cmdline")
#define MAX_COMMAND_LINE_SIZE 4096

static int unlink_failure(const char *path)
{
	fprintf(stderr, "Cannot unlink %s: %s\n",
		path, strerror(errno));
	return -1;
}

static int mount_failure(const char *what)
{
	fprintf(stderr, "Cannot mount %s: %s\n", what, strerror(errno));
	return -1;
}

static int chdir_failure(const char *path)
{
	fprintf(stderr, "Cannot change directory to %s: %s\n",
		path, strerror(errno));
	return -1;
}

static int execute(const char *prog, char *const *argv)
{
	execv(prog, argv);
	fprintf(stderr, "Cannot execute %s: %s\n",
		prog, strerror(errno));
	return -1;
}

static int almost_all(const struct dirent *entry)
{
	const char *p = entry->d_name;
	if (p[0] == '.' && (p[1] == '\0' || (p[1] == '.' && p[2] == '\0')))
		return 0;
	return 1;
}

static int remove_subdirectory(dev_t dev, ino_t skip_ino,
			       int fd, const char *path);

static int remove_directory(dev_t dev, ino_t skip_ino, DIR *dir)
{
	struct dirent *entry;
	struct stat st;
	int fd;

	fd = dirfd(dir);
	while (1) {
		errno = 0;
		entry = readdir(dir);
		if (!entry)
			break;

		if (!almost_all(entry))
			continue;

		if (entry->d_type != DT_DIR) {
			if (unlinkat(fd, entry->d_name, 0) == 0)
				continue;
			if (errno != EISDIR) {
				unlink_failure(entry->d_name);
				continue;
			}
		}

		if (fstatat(fd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW)) {
			fprintf(stderr, "Cannot stat %s: %s\n",
				entry->d_name, strerror(errno));
			continue;
		}
		if (st.st_dev != dev)
			continue;
		if (st.st_ino == skip_ino)
			continue;

		remove_subdirectory(dev, skip_ino, fd, entry->d_name);
		if (unlinkat(fd, entry->d_name, AT_REMOVEDIR))
			unlink_failure(entry->d_name);
	}

	if (errno) {
		fprintf(stderr, "Cannot read directory: %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}

static int remove_subdirectory(dev_t dev, ino_t skip_ino,
			       int dirfd, const char *path)
{
	DIR *subdir;
	int subfd;
	int err;

	subfd = openat(dirfd, path, O_RDONLY);
	if (subfd < 0) {
		fprintf(stderr, "Cannot open directory %s: %s\n",
			path, strerror(errno));
		return -1;
	}

	subdir = fdopendir(subfd);
	if (!subdir) {
		fprintf(stderr, "fdopendir() failed on %s: %s\n",
			path, strerror(errno));
		close(subfd);
		return -1;
	}

	err = remove_directory(dev, skip_ino, subdir);
	closedir(subdir);
	return err;
}

static int remove_subtree(const char *path, const char *skip)
{
	struct stat st;
	ino_t skip_ino;
	DIR *dir;
	int err;

	if (stat(skip, &st)) {
		fprintf(stderr, "Cannot stat %s: %s\n",
			skip, strerror(errno));
		return -1;
	}
	skip_ino = st.st_ino;

	dir = opendir(path);
	if (!dir) {
		fprintf(stderr, "Cannot open directory %s: %s\n",
			path, strerror(errno));
		return -1;
	}

	err = fstat(dirfd(dir), &st);
	if (err < 0) {
		fprintf(stderr, "Cannot stat directory %s: %s\n",
			path, strerror(errno));
	} else {
		err = remove_directory(st.st_dev, skip_ino, dir);
	}
	closedir(dir);

	return err;
}

static int check_initramfs(const char *path)
{
	struct statfs buf;
	int err;

	err = statfs("/", &buf);
	if (err) {
		fprintf(stderr, "Cannot get filesystem type: %s",
			strerror(errno));
		return -1;
	}

	if (buf.f_type != RAMFS_MAGIC && buf.f_type != TMPFS_MAGIC) {
		fprintf(stderr, "%s is neither ramfs nor tmpfs", path);
		return -1;
	}

	return 0;
}

/* copy kernel command line to the SYSTEMD_PROC_CMDLINE environment variable,
 * replacing root=... with root=kdump rootflags=bind
 */
static int adjust_cmdline()
{
	FILE *in;
	int c, in_quotes = 0, filter = 0;
	const char match[] = "root=";
	const char *match_pos = match;
	char cmdline[MAX_COMMAND_LINE_SIZE];
	char *cmdline_pos = cmdline;
	char *cmdline_end = cmdline + MAX_COMMAND_LINE_SIZE - 1;
	int truncated = 0;

	in = fopen(KERNEL_CMDLINE, "r");
	if (!in) {
		fprintf(stderr, "Cannot open %s: %s\n",
			KERNEL_CMDLINE, strerror(errno));
		return -1;
	}

	/* put root=kdump rootflags=bind on the beginning of cmdline */
	cmdline_pos = stpcpy(cmdline_pos, "root=kdump rootflags=bind ");

	/* copy original command line, filtering out root= */
	while ((c = fgetc(in)) != EOF) {

		if (c == '"')
			in_quotes = !in_quotes;

		if (isspace(c) && !in_quotes)
			/* new option, start matching */
			match_pos = match;
		else if (match_pos) {
			if (c == *match_pos) {
				/* c is still matching */
				filter = 1;
				++match_pos;
				if (! *match_pos)
					match_pos = NULL; /* matched until the end of match */

			} else {
				/* c does not match */
				const char *m = match;
				filter = 0;

				/* output the already matched part */
				for (m = match; m < match_pos; ++m) {
					if (cmdline_pos >= cmdline_end) {
						truncated = 1;
						break;
					}
					*(cmdline_pos++) = *m;
				}

				/* stop filtering, stop matching until next space */
				filter = 0;
				match_pos = NULL;
			}
		}

		if (!filter) {
			if (cmdline_pos >= cmdline_end) {
				truncated = 1;
				break;
			}
			*(cmdline_pos++) = c;
		}
	}

	fclose(in);

	*cmdline_pos = 0;
	printf("Passing modified command line to systemd: %s\n", cmdline);
	if (truncated)
		fprintf(stderr, "Warning, command line truncated!\n");

	if (setenv("SYSTEMD_PROC_CMDLINE", cmdline, 1)) {
		fprintf(stderr, "Failed setting SYSTEMD_PROC_CMDLINE: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static int exec_next_init(int argc, char *const *argv)
{
	int fadump = 0;

	/* temporarily mount /proc to:
	 * - check for /proc/vmcore to decide if we're in a fadump environment
	 * - pass a modified command line to systemd, replacing root=... with root=kdump
	 */
	if (mount(PROC, PROC_DIR, PROC,
		  MS_NOSUID|MS_NOEXEC|MS_NODEV, NULL))
		mount_failure(PROC);
	else {
		if (access(PROC_VMCORE, F_OK) == 0) {
			fadump = 1;
			adjust_cmdline();
		}

		/* give a warning, but ignore errors */
		if (umount(PROC_DIR))
			fprintf(stderr, "Cannot unmount %s: %s\n",
				PROC_DIR, strerror(errno));
	}

	if (!fadump) {
                if (rename(DRACUT_INIT, argv[0])) {
                        fprintf(stderr, "Cannot rename %s to %s: %s\n",
                                DRACUT_INIT, argv[0], strerror(errno));
                        return execute(DRACUT_INIT, argv);
                }
                return execute(argv[0], argv);
        }

	if (check_initramfs("/"))
		return -1;

	/* ignore errors */
	remove_subtree("/", FADUMP_DIR);

	if (mount(FADUMP_DIR, FADUMP_DIR, NULL, MS_BIND, NULL))
		return mount_failure(FADUMP_DIR " over itself");

	if (chdir(FADUMP_DIR))
		return chdir_failure(FADUMP_DIR);

	if (mount(FADUMP_DIR, "/", NULL, MS_MOVE, NULL))
		return mount_failure(FADUMP_DIR " over /");

	if (chroot(".")) {
		fprintf(stderr, "Cannot change root: %s\n",
			strerror(errno));
		return -1;
	}
	if (chdir("/"))
		return chdir_failure("new root");

	return execute(argv[0], argv);
}

int main(int argc, char **argv)
{
	exec_next_init(argc, argv);

	fprintf(stderr, "No way to continue. Kernel panic expected!\n");
	return 1;
}

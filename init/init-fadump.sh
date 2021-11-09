#!/bin/sh
export PATH=/usr/bin:/usr/sbin
export SYSTEMD_IN_INITRD=lenient

[ -e /proc/mounts ] ||
	(mkdir -p /proc && mount -t proc -o nosuid,noexec,nodev proc /proc)

grep -q '^sysfs /sys sysfs' /proc/mounts ||
	(mkdir -p /sys && mount -t sysfs -o nosuid,noexec,nodev sysfs /sys)

if [ -f /proc/device-tree/rtas/ibm,kernel-dump ] || [ -f /proc/device-tree/ibm,opal/dump/mpipl-boot ]; then
	mkdir /newroot
	mount -t ramfs ramfs /newroot

	for FILE in $(ls -A /fadumproot/); do
		mv /fadumproot/$FILE /newroot/
	done
	exec switch_root /newroot /init
else
	exec /init.dracut
fi

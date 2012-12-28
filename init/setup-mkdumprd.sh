#!/bin/bash
#
#%stage: boot
#%depends: done
#

if ! (( $use_kdump )) ; then
    if chkconfig -c boot.kdump B ; then
        # Avoid warnings about non-existent path, because
        # $tmp_mnt was already deleted in setup-done.sh
        cd "$oldpwd"
        mkdumprd -f -K "${kernel_image}" -I "${initrd_image}-kdump"
    fi
fi

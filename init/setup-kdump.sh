#!/bin/bash
#
#%stage: crypto
#%provides: kdump

#
# copy /etc/sysconfig/kdump
#

if [ ! -f /etc/sysconfig/kdump ] ; then
    echo "kdump configuration not installed"
    return 1
fi

mkdir -p ${tmp_mnt}/etc/sysconfig/
cp /etc/sysconfig/kdump ${tmp_mnt}/etc/sysconfig/

#
# remember the host name
#
hostname --fqdn >> ${tmp_mnt}/etc/hostname.kdump

use_kdump=
if use_script kdump ; then
    use_kdump=1
fi

save_var use_kdump

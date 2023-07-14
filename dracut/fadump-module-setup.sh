#!/bin/bash

# this module is part of kdump
# for FADUMP, the kdump initrd is embedded in every system initrd
# A modified /init program determines whether to run the standard init
# or the one from the embedded kdump initrd

test -n "$KDUMP_LIBDIR" || KDUMP_LIBDIR=/usr/lib/kdump
. "$KDUMP_LIBDIR"/kdump-read-config.sh || return 1

check() {
    if test "$KDUMP_FADUMP" = "true"; then
        return 0
    fi

    return 255
}

depends() {
    return 0
}

install() {
    local _fadumpdir="$initdir/fadumproot"

    mkdir -p "$_fadumpdir" || return 1

    dinfo "****** Generating FADUMP initrd *******"
    mkdumprd -F -I "$_fadumpdir/initrd" -K "$kernel" || return 1
    pushd "$_fadumpdir" || return 1
    lsinitrd --unpack initrd || return 1
    rm -f initrd
    popd
    dinfo "****** Generating FADUMP initrd done *******"

    mv -f "$initdir/init" "$initdir/init.dracut"
    inst_binary "$moddir/init-fadump" /init
    chmod a+x "$initdir/init"
}

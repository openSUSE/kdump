#!/bin/bash

. /usr/lib/kdump/setup-kdump.functions

check() {
    if kdump_get_config && test "$KDUMP_FADUMP" = "yes"; then
        return 0
    fi

    return 255
}

depends() {
    return 0
}

install() {
    kdump_get_config || return 1

    local _fadumpdir="$initdir/fadumproot"

    mkdir -p "$_fadumpdir" || return 1

    dinfo "****** Generating FADUMP initrd *******"
    local _initrd="$_fadumpdir/initrd"
    local -a _dracut_args=(
        "--force"
        "--hostonly"
        "--add" "kdump"
        "--omit" "plymouth resume usrmount zz-fadumpinit"
        "--no-compress"
        "--no-early-microcode"
    )
    local _dracut="${dracut_cmd:-dracut}"
    "$_dracut" "${_dracut_args[@]}" "$_initrd" "$kernel" || return 1
    kdump_unpack_initrd "$_initrd" "$_fadumpdir" || return 1
    rm -f "$_initrd"
    dinfo "****** Generating FADUMP initrd done *******"

    mv -f "$initdir/init" "$initdir/init.dracut"
    inst_binary "$moddir/init-fadump" /init
    chmod a+x "$initdir/init"
}

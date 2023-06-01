#!/bin/bash

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
    local _initrd="$_fadumpdir/initrd"
    local -a _dracut_args=(
        "--force"
        "--hostonly"
        "--add" "kdump"
        "--omit" "plymouth resume usrmount zz-fadumpinit"
        "--no-compress"
        "--no-early-microcode"
    )

    [[ -n "${KDUMP_DRACUT_MOUNT_OPTION}" ]] && _dracut_args+=("--mount" "${KDUMP_DRACUT_MOUNT_OPTION}")

    local _dracut="${dracut_cmd:-dracut}"
    "$_dracut" "${_dracut_args[@]}" "$_initrd" "$kernel" || return 1
    pushd "$_fadumpdir" || return 1
    lsinitrd --unpack "$_initrd" || return 1
    popd
    rm -f "$_initrd"
    dinfo "****** Generating FADUMP initrd done *******"

    mv -f "$initdir/init" "$initdir/init.dracut"
    inst_binary "$moddir/init-fadump" /init
    chmod a+x "$initdir/init"
}

#!/bin/bash

check() {
    return 255
}

depends() {
    return 0
}

install() {
    mv -f "$initdir/init" "$initdir/init.dracut"
    inst_script "$moddir/init-fadump" /init
    chmod a+x "$initdir/init"

    # Install required binaries for the init script (init-fadump.sh)
    inst_multiple sh modprobe grep mkdir mount ls mv switch_root
}

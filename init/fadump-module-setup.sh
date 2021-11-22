#!/bin/bash

check() {
    return 255
}

depends() {
    return 0
}

install() {
    mv -f "$initdir/init" "$initdir/init.dracut"
    inst_binary "$moddir/init-fadump" /init
    chmod a+x "$initdir/init"
}

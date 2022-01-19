#!/bin/bash
# -*- mode: shell-script; indent-tabs-mode: nil; sh-basic-offset: 4; -*-
#
# mount-kdump.sh: fake successful mount without mounting anything
#
# The kdump module adds all necessary mount points under /kdump,
# so there is no need to mount root separately. In fact, mounting
# the root filesystem may not be needed at all, e.g. with FTP dump.
#

# override ismounted: pretend that "$NEWROOT" is mounted
# since this function must return correct result for non-root fs,
# let's modify the existing definition using sed:
eval "$(
    type ismounted |
    sed -e '1,/^{/s/^{.*/&\n    test "$1" != "$NEWROOT" || return 0/; 1d'
)"

# override usable_root: everything is "usable" for kdump
usable_root()
{
    return 0
}

# vim: set sw=4 ts=4 et:

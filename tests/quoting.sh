#!/bin/bash
#
# (c) 2013, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

#
# Program                                                                    {{{
#

KDUMPTOOL=$1
DIR=$2

if [ -z "$DIR" ] || [ -z "$KDUMPTOOL" ] ; then
    echo "Usage: $0 kdumptool directory"
    exit 1
fi

errors=0
output=$( $KDUMPTOOL -F $DIR/quoting.conf dump_config )
status=$?
if [ $status -ne 0 ] ; then
    echo "Exitstatus is '$status'"
    errors=$[$errors+1]
fi

eval "$output"

exp_KDUMP_COMMANDLINE="two words"
exp_KDUMP_TRANSFER=\"
exp_KDUMP_SAVEDIR=\"That\'s\ impossible\!\"\ he\ said.

if [ "$KDUMP_COMMANDLINE" != "$exp_KDUMP_COMMANDLINE" ] ; then
    echo "\$KDUMP_COMMANDLINE must be '$exp_KDUMP_COMMANDLINE', but is '$KDUMP_COMMANDLINE'."
    errors=$[$errors+1]
fi

if [ "$KDUMP_TRANSFER" != "$exp_KDUMP_TRANSFER" ] ; then
    echo "\$KDUMP_TRANSFER must be '$exp_KDUMP_TRANSFER', but is '$KDUMP_TRANSFER'."
    errors=$[$errors+1]
fi

if [ "$KDUMP_SAVEDIR" != "$exp_KDUMP_SAVEDIR" ] ; then
    echo "\$KDUMP_SAVEDIR must be '$exp_KDUMP_SAVEDIR', but is '$KDUMP_SAVEDIR'."
    errors=$[$errors+1]
fi

exit $errors

# }}}

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

#!/bin/bash
#
# (c) 2008, Bernhard Walle <bwalle@suse.de>, SUSE LINUX Products GmbH
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

TESTCONFIG=$1
FILE=$2

if [ -z "$FILE" ] || [ -z "$TESTCONFIG" ] ; then
    echo "Usage: $0 testio config"
    exit 1
fi

errors=0

FOO=$($TESTCONFIG $FILE FOO 2>/dev/null)
errors=$[$errors + $?]

BAR=$($TESTCONFIG $FILE BAR 2>/dev/null)
errors=$[$errors + $?]

TEST=$($TESTCONFIG $FILE TEST 2>/dev/null)
errors=$[$errors + $?]

QUOTING=$($TESTCONFIG $FILE QUOTING 2>/dev/null)
errors=$[$errors + $?]

if [ "$FOO" != "bar" ] ; then
    echo "\$FOO must be 'bar' but is '$FOO'."
    errors=$[$errors + 1]
fi

if [ "$BAR" != "2" ] ; then
    echo "\$BAR must be '2' but is '$BAR'."
    errors=$[$errors + 1]
fi

if [ "$TEST" != "bar" ] ; then
    echo "\$TEST must be 'bar' but is '$TEST'."
    errors=$[$errors + 1]
fi

if [ "$QUOTING" != '$FOO' ] ; then
    echo "\$QUOTING must be '\$FOO' but is '$QUOTING'."
    errors=$[$errors + 1]
fi

exit $errors

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

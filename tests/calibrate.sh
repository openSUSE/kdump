#!/bin/bash
#
# (c) 2014, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
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

KDUMPTOOL=$1
DIR=$2
KDUMPOPT="-F $2/empty.conf"

if [ -z "$DIR" ] || [ -z "$KDUMPTOOL" ] ; then
    echo "Usage: $0 kdumptool directory"
    exit 1
fi

errors=0

REQMEM=$($KDUMPTOOL $KDUMPOPT calibrate)
test $? == 0 || errors=1

if [ -z "$REQMEM" ] ; then
    echo "calibrate did not produce any output"
    errors=$(( $errors + 1 ))
elif [ "$REQMEM" -ne "$REQMEM" ] 2> /dev/null ; then
    echo "calibrate should print an integer number, got '$REQMEM'"
    errors=$(( $errors + 1 ))
elif [ "$REQMEM" -lt 0 ] ; then
    echo "calibrate should not return negative number, got '$REQMEM'"
    errors=$(( $errors + 1 ))
fi

exit $errors

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

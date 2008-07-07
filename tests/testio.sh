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

TESTIO=$1
DIR=$2

if [ -z "$DIR" ] || [ -z "$TESTIO" ] ; then
    echo "Usage: $0 testio directory"
    exit 1
fi

errors=0
$TESTIO $DIR/test.txt $DIR/test.txt.1 $DIR/test.txt.2

cmp $DIR/test.txt $DIR/test.txt.1
if [ $? -ne 0 ] ; then
    echo "$DIR/test.txt != $DIR/test.txt.1"
    errors=$[$errors+1]
fi

cmp $DIR/test.txt $DIR/test.txt.2
if [ $? -ne 0 ] ; then
    echo "$DIR/test.txt != $DIR/test.txt.2"
    errors=$[$errors+1]
fi

rm -f $DIR/test.txt.1 $DIR/test.txt.2
exit $errors

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:

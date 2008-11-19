#!/bin/sh
# (c) 2008, Bernhard Walle <bwalle@suse.de>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

VERSION=$(grep '^set (PACKAGE_VERSION' CMakeLists.txt | \
          sed -e 's/.*PACKAGE_VERSION "\([0-9.]*\).*/\1/g')

TARBALL=kdump-${VERSION}.tar.bz2

#
# Generate tarball
#
hg archive -X tests -X create_release.sh -t tbz2 $TARBALL

#
# Test build
#
if [ "$1" != "notest" ] ; then
    dir=$PWD
    TEMPBUILD=$PWD/tempbuild-$$
    mkdir $TEMPBUILD
    cd $TEMPBUILD
    tar xf ../$TARBALL
    mkdir build
    cmake ..
    make -j2
    cd $dir
    rm -fr $TEMPBUILD
fi

echo "-------------------------------"
echo "$TARBALL ready."
echo "-------------------------------"

exit 0

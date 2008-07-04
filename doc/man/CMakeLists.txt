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

ADD_CUSTOM_TARGET(kdumptool_8
    ALL
    COMMAND
        ${A2X_EXECUTABLE} ${A2X_MAN_OPTIONS}
        -D ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/kdumptool.8.txt &>/dev/null
    DEPENDS
        kdumptool.8.txt
    COMMENT
        "Building kdumptool(8) manpage"
)

ADD_CUSTOM_TARGET(kdump_5
    ALL
    COMMAND
        ${A2X_EXECUTABLE} ${A2X_MAN_OPTIONS}
        -D ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/kdump.5.txt
    DEPENDS
        kdump.5.txt
    COMMENT
        "Building kdump(5) manpage"
)


# vim: set sw=4 ts=4 et:

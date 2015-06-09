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

ADD_EXECUTABLE(device-timeout-generator
    device-timeout-generator.c
)
TARGET_INCLUDE_DIRECTORIES(device-timeout-generator
    PUBLIC ${BLKID_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(device-timeout-generator
    ${BLKID_LIBRARIES})

INSTALL(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/boot.kdump
    DESTINATION
        /etc/init.d
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

INSTALL(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/setup-mkdumprd.sh
        ${CMAKE_CURRENT_SOURCE_DIR}/setup-kdumpfs.sh
        ${CMAKE_CURRENT_SOURCE_DIR}/setup-kdump.sh
        ${CMAKE_CURRENT_SOURCE_DIR}/boot-kdump.sh
    DESTINATION
        /lib/mkinitrd/scripts
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

INSTALL(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/mkdumprd
        ${CMAKE_CURRENT_SOURCE_DIR}/kdump-bootloader.pl
    DESTINATION
        /usr/sbin
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

INSTALL(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/rc.kdump.functions
        ${CMAKE_CURRENT_SOURCE_DIR}/setup-kdump.functions
        ${CMAKE_CURRENT_SOURCE_DIR}/save_dump.sh
    DESTINATION
        /lib/kdump
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

INSTALL(
    TARGETS
        device-timeout-generator
    DESTINATION
        /usr/lib/dracut/modules.d/99kdump
)

INSTALL(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/module-setup.sh
        ${CMAKE_CURRENT_SOURCE_DIR}/mount-kdump.sh
    DESTINATION
        /usr/lib/dracut/modules.d/99kdump
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

INSTALL(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/kdump-save.service
    DESTINATION
        /usr/lib/dracut/modules.d/99kdump
    PERMISSIONS
        OWNER_READ OWNER_WRITE
        GROUP_READ
        WORLD_READ
)

# vim: set sw=4 ts=4 et:

#
# Copyright (c) 2022 SUSE LLC
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
# along with this program; if not, see <https://www.gnu.org/licenses>.
#

ADD_EXECUTABLE(device-timeout-generator
    device-timeout-generator.c
)
TARGET_INCLUDE_DIRECTORIES(device-timeout-generator
    PUBLIC ${BLKID_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(device-timeout-generator
    ${BLKID_LIBRARIES})

ADD_EXECUTABLE(kdump-save
    kdump-save.cc
)
TARGET_INCLUDE_DIRECTORIES(kdump-save
    PUBLIC ${CMAKE_SOURCE_DIR}/kdumptool)
TARGET_LINK_LIBRARIES(kdump-save common ${EXTRA_LIBS})

INSTALL(
    TARGETS
        device-timeout-generator
    DESTINATION
        /usr/lib/dracut/modules.d/99kdump
)

INSTALL (
    TARGETS
        kdump-save
    DESTINATION
        /usr/lib/kdump
)

INSTALL(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/setup-kdump.functions
    DESTINATION
        /usr/lib/kdump
    PERMISSIONS
        OWNER_READ OWNER_WRITE
        GROUP_READ
        WORLD_READ
)

INSTALL(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/kdump-root.sh
        ${CMAKE_CURRENT_SOURCE_DIR}/kdump-boot.sh
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
        ${CMAKE_CURRENT_SOURCE_DIR}/kdump-save.service.in
    DESTINATION
        /usr/lib/dracut/modules.d/99kdump
    PERMISSIONS
        OWNER_READ OWNER_WRITE
        GROUP_READ
        WORLD_READ
)

IF(${HAVE_FADUMP} STREQUAL "TRUE")

    INSTALL(
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/fadump-module-setup.sh
        RENAME
            module-setup.sh
        DESTINATION
            /usr/lib/dracut/modules.d/99zz-fadumpinit
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
    )

    ADD_EXECUTABLE(init-fadump
        init-fadump.c
    )
    INSTALL(
        TARGETS
            init-fadump
        DESTINATION
            /usr/lib/dracut/modules.d/99zz-fadumpinit
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
    )

ENDIF()

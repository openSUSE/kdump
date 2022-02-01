#
# Copyright (c) 2021 SUSE LLC
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

ADD_EXECUTABLE(trackrss
    trackrss.c
)
target_link_libraries(trackrss -lpthread)

ADD_EXECUTABLE(mkelfcorehdr
    mkelfcorehdr.c
)

get_directory_property(dracut_targets
    DIRECTORY ${CMAKE_SOURCE_DIR}/dracut
    BUILDSYSTEM_TARGETS
)

ADD_CUSTOM_COMMAND(
    OUTPUT
        calibrate.conf
    COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/run-qemu.py
        > ${CMAKE_CURRENT_BINARY_DIR}/calibrate.conf
    VERBATIM
    DEPENDS
        ${dracut_targets}
        kdumptool
        dummy.conf
        dummy-net.conf
        trackrss
        mkelfcorehdr
        kernel.py
        maxrss.py
        run-qemu.py
)

IF(CALIBRATE)

    ADD_CUSTOM_TARGET(calibrate-conf
        ALL
        DEPENDS
            calibrate.conf
    )
    INSTALL(
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/calibrate.conf
        DESTINATION
            /usr/lib/kdump
    )

ENDIF()

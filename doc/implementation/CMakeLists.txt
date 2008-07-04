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

ADD_CUSTOM_TARGET(implementation_doc
    ALL
    COMMAND
        ${ASCIIDOC_EXECUTABLE} ${ASCIIDOC_OPTIONS}
        -o ${CMAKE_CURRENT_BINARY_DIR}/Implementation_Guide.html
        ${CMAKE_CURRENT_SOURCE_DIR}/Implementation_Guide.txt
    DEPENDS
        Implementation_Guide.txt
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT
        "Building implementation documentation"
)

# vim: set sw=4 ts=4 et:

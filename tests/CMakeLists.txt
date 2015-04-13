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

ENABLE_TESTING()
ADD_TEST(kdumptool_identify_kernel
         ${CMAKE_CURRENT_SOURCE_DIR}/identify_kernel.sh
         ${CMAKE_BINARY_DIR}/kdumptool/kdumptool
         ${CMAKE_CURRENT_SOURCE_DIR}/data)

ADD_TEST(testio
         ${CMAKE_CURRENT_SOURCE_DIR}/testio.sh
         ${CMAKE_BINARY_DIR}/kdumptool/testio
         ${CMAKE_CURRENT_SOURCE_DIR}/data)

ADD_TEST(testconfig
         ${CMAKE_CURRENT_SOURCE_DIR}/testconfig.sh
         ${CMAKE_BINARY_DIR}/kdumptool/testconfig
         ${CMAKE_CURRENT_SOURCE_DIR}/data/configuration.txt)

ADD_TEST(urlparser
         ${CMAKE_CURRENT_SOURCE_DIR}/testurlparser.sh
         ${CMAKE_BINARY_DIR}/kdumptool/testurlparser)

ADD_TEST(quoting
         ${CMAKE_CURRENT_SOURCE_DIR}/quoting.sh
         ${CMAKE_BINARY_DIR}/kdumptool/kdumptool
         ${CMAKE_CURRENT_SOURCE_DIR}/data)

ADD_TEST(calibrate
         ${CMAKE_CURRENT_SOURCE_DIR}/calibrate.sh
         ${CMAKE_BINARY_DIR}/kdumptool/kdumptool
         ${CMAKE_CURRENT_SOURCE_DIR}/data)

ADD_TEST(listdir
         ${CMAKE_CURRENT_SOURCE_DIR}/listdir.sh
         ${CMAKE_BINARY_DIR}/kdumptool/testlistdir
         ${CMAKE_CURRENT_SOURCE_DIR}/data)

ADD_TEST(delete_dumps
         ${CMAKE_CURRENT_SOURCE_DIR}/delete_dumps.sh
         ${CMAKE_BINARY_DIR}/kdumptool/kdumptool
         ${CMAKE_CURRENT_SOURCE_DIR}/data)

ADD_TEST(process
         ${CMAKE_CURRENT_SOURCE_DIR}/process.sh
         ${CMAKE_BINARY_DIR}/kdumptool/testprocess)

ADD_TEST(urldecode
         ${CMAKE_CURRENT_SOURCE_DIR}/testurldecode.sh
         ${CMAKE_BINARY_DIR}/kdumptool/testurldecode)

ADD_TEST(sftppacket
         ${CMAKE_CURRENT_SOURCE_DIR}/testsftppacket.sh
         ${CMAKE_BINARY_DIR}/kdumptool/testsftppacket)

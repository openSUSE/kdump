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

SET(KDUMPTOOL_SRC
    optionparser.cc
    optionparser.h
    subcommand.h
    subcommand.cc
    kdumptool.h
    kdumptool.cc
    identifykernel.h
    identifykernel.cc
    configuration.h
    configuration.cc
    configparser.h
    configparser.cc
    process.h
    process.cc
    util.h
    util.cc
    stringutil.h
    stringutil.cc
    global.cc
    global.h
    debug.cc
    debug.h
    urlparser.cc
    urlparser.h
    dataprovider.cc
    dataprovider.h
    fileutil.cc
    fileutil.h
    transfer.cc
    transfer.h
    ledblink.cc
    ledblink.h
    progress.cc
    progress.h
    main.cc
)

add_executable(kdumptool ${KDUMPTOOL_SRC})
target_link_libraries(kdumptool ${EXTRA_LIBS})
install (TARGETS kdumptool DESTINATION sbin)

SET(TESTIO_SRC
    optionparser.cc
    optionparser.h
    process.h
    process.cc
    util.h
    util.cc
    stringutil.h
    stringutil.cc
    global.cc
    global.h
    debug.cc
    debug.h
    testio.cc
)

add_executable(testio ${TESTIO_SRC})
target_link_libraries(testio ${EXTRA_LIBS})

SET(TESTCONFIG_SRC
    configparser.cc
    configparser.h
    util.h
    util.cc
    process.cc
    process.h
    stringutil.h
    stringutil.cc
    global.cc
    global.h
    debug.cc
    debug.h
    testconfig.cc
)

add_executable(testconfig ${TESTCONFIG_SRC})
target_link_libraries(testconfig ${EXTRA_LIBS})


SET(TESTURLPARSER_SRC
    urlparser.cc
    urlparser.h
    util.h
    util.cc
    process.cc
    process.h
    stringutil.h
    stringutil.cc
    global.cc
    global.h
    debug.cc
    debug.h
    testurlparser.cc
)

add_executable(testurlparser ${TESTURLPARSER_SRC})
target_link_libraries(testurlparser ${EXTRA_LIBS})

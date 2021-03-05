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

SET(COMMON_SRC
    option.cc
    option.h
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
    charv.h
    charv.cc
    stringutil.h
    stringutil.cc
    global.cc
    global.h
    debug.cc
    debug.h
    urlparser.cc
    urlparser.h
    rootdirurl.cc
    rootdirurl.h
    dataprovider.cc
    dataprovider.h
    fileutil.cc
    fileutil.h
    transfer.cc
    transfer.h
    sshtransfer.cc
    sshtransfer.h
    socket.cc
    socket.h
    ledblink.cc
    ledblink.h
    mounts.cc
    mounts.h
    progress.cc
    progress.h
    savedump.cc
    savedump.h
    vmcoreinfo.cc
    vmcoreinfo.h
    read_vmcoreinfo.cc
    read_vmcoreinfo.h
    print_target.cc
    print_target.h
    email.cc
    email.h
    deletedumps.h
    deletedumps.cc
    kconfig.h
    kconfig.cc
    kerneltool.h
    kerneltool.cc
    read_ikconfig.h
    read_ikconfig.cc
    findkernel.cc
    findkernel.h
    dumpconfig.cc
    dumpconfig.h
    quotedstring.cc
    quotedstring.h
    multipath.cc
    multipath.h
    calibrate.cc
    calibrate.h
    routable.cc
    routable.h
)

add_library(common STATIC ${COMMON_SRC})

add_executable(kdumptool
    main.cc
)
target_link_libraries(kdumptool common ${EXTRA_LIBS})
install (TARGETS kdumptool DESTINATION sbin)

add_executable(testio
    testio.cc
)
target_link_libraries(testio common ${EXTRA_LIBS})

add_executable(testconfig
    testconfig.cc
)
target_link_libraries(testconfig common ${EXTRA_LIBS})

add_executable(teststringutil
    teststringutil.cc
)
target_link_libraries(teststringutil common ${EXTRA_LIBS})

add_executable(testurlparser
    testurlparser.cc
)
target_link_libraries(testurlparser common ${EXTRA_LIBS})

add_executable(testmail
    testmail.cc
)
target_link_libraries(testmail common ${EXTRA_LIBS})

add_executable(testreadlink
    testreadlink.cc
)
target_link_libraries(testreadlink common ${EXTRA_LIBS})

add_executable(testkconfig
    testkconfig.cc
)
target_link_libraries(testkconfig common ${EXTRA_LIBS})

add_executable(testcanonical
    testcanonical.cc
)
target_link_libraries(testcanonical common ${EXTRA_LIBS})

add_executable(testlistdir
    testlistdir.cc
)
target_link_libraries(testlistdir common ${EXTRA_LIBS})

add_executable(testprocess
    testprocess.cc
)
target_link_libraries(testprocess common ${EXTRA_LIBS})

add_executable(testurldecode
    testurldecode.cc
)
target_link_libraries(testurldecode common ${EXTRA_LIBS})

add_executable(testsftppacket
    testsftppacket.cc
)
target_link_libraries(testsftppacket common ${EXTRA_LIBS})

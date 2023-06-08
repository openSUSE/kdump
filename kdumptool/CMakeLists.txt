ADD_EXECUTABLE(calibrate
    calibrate.cc
)

INSTALL(
    TARGETS
        calibrate
    DESTINATION
        /usr/lib/kdump
)

INSTALL(
    FILES
        kdumptool
    DESTINATION
        /usr/sbin
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

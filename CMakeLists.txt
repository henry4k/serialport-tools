cmake_minimum_required(VERSION 2.8)
include(FindPkgConfig)

pkg_check_modules(LIBSERIALPORT REQUIRED "libserialport >= 0.1.0")
include_directories(${LIBSERIALPORT_INCLUDE_DIRS})
link_directories(${LIBSERIALPORT_LIBRARY_DIRS})

pkg_check_modules(POPT REQUIRED "popt >= 1.16")
include_directories(${POPT_INCLUDE_DIRS})
link_directories(${POPT_LIBRARY_DIRS})

project(serialport-tools)

add_executable(serialport-pipe serialport-pipe.c shared.c)
target_link_libraries(serialport-pipe ${LIBSERIALPORT_LIBRARIES} ${POPT_LIBRARIES})

add_executable(serialport-list serialport-list.c shared.c)
target_link_libraries(serialport-list ${LIBSERIALPORT_LIBRARIES} ${POPT_LIBRARIES})

install(TARGETS serialport-pipe
        RUNTIME DESTINATION bin)

install(TARGETS serialport-list
        RUNTIME DESTINATION bin)

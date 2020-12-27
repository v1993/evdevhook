cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 20)

project(evdevhook
DESCRIPTION "DSU server for motion from evdev compatible joysticks"
LANGUAGES C CXX)

find_package(PkgConfig)

pkg_check_modules(libevdev REQUIRED IMPORTED_TARGET libevdev)
pkg_check_modules(libudev REQUIRED IMPORTED_TARGET libudev)
pkg_check_modules(glibmm REQUIRED IMPORTED_TARGET glibmm-2.4)
pkg_check_modules(giomm REQUIRED IMPORTED_TARGET giomm-2.4)

find_package(nlohmann_json 3.9.0 REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(evdevhook
	src/constants.hpp
	src/globals.hpp
	src/main.cpp
	src/packet.cpp
	src/packet.hpp
	src/VirtualDevice.cpp
	src/VirtualDevice.hpp
)

target_link_libraries(evdevhook
	PkgConfig::libevdev PkgConfig::libudev # Sorta obvious
	PkgConfig::glibmm PkgConfig::giomm # Networking and I/O management
	nlohmann_json::nlohmann_json # Config
	ZLIB::ZLIB # CRC32 calculation
)

# Installation
include(GNUInstallDirs)
install(TARGETS evdevhook DESTINATION "${CMAKE_INSTALL_BINDIR}")

# CPack
set(CPACK_PACKAGE_NAME "evdevhook")
set(CPACK_PACKAGE_VENDOR "Valeri Ochinski")
set(CPACK_PACKAGE_CONTACT "Valeri Ochinski v19930312@gmail.com")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_EXECUTABLES "evdevhook")
set(CPACK_STRIP_FILES TRUE)

# CPack - DEB specific
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/v1993/evdevhook")
include(CPack)

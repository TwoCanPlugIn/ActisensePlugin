##---------------------------------------------------------------------------
## Author:      Steven Adler (based on standard OpenCPN Plug-In CMAKE commands)
## Copyright:   2018
## License:     GPL v3+
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(actisense_pi)

SET(PACKAGE_NAME actisense_pi)
SET(VERBOSE_NAME ACTISENSE)
SET(TITLE_NAME ACTISENSE)
SET(CPACK_PACKAGE_CONTACT "TwoCanPlugin@hotmail.com")
# SET(PLUGIN_VERSION_MAJOR "1")
# SET(PLUGIN_VERSION_MINOR "0")
# SET(PLUGIN_VERSION_PATCH "0")
# SET(NAME_SUFFIX "ov50")
# SET(PLUGIN_VERSION_DATE "2019-12-10")
SET(CMAKE_BUILD_TYPE Release)
SET(CMAKE_CXX_STANDARD 11)

INCLUDE("cmake/PluginConfigure.cmake")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/inc ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/img)

SET(SRC_ACTISENSE
            src/twocanerror.cpp
            inc/twocanerror.h
            src/twocanutils.cpp
            inc/twocanutils.h
            src/actisense_device.cpp
            inc/actisense_device.h
            src/actisense_icons.cpp
            inc/actisense_icons.h
            src/actisense_plugin.cpp
            inc/actisense_plugin.h
            src/actisense_settings.cpp
            inc/actisense_settings.h
            src/actisense_settingsbase.cpp
            inc/actisense_settingsbase.h
            src/actisense_interface.cpp
            inc/actisense_interface.h
            src/actisense_ebl.cpp
            inc/actisense_ebl.h
            src/actisense_ngt1.cpp
            inc/actisense_ngt1.h
            inc/version.h
            img/actisense_white.png
            img/actisense_logo_32.png
            img/actisense_logo_16.png
 	)

ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_ACTISENSE} )

INCLUDE("cmake/PluginInstall.cmake")
INCLUDE("cmake/PluginLocalization.cmake")
INCLUDE("cmake/PluginPackage.cmake")


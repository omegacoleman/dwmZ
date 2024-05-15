CMAKE_MINIMUM_REQUIRED( VERSION 3.13 )

PROJECT( agg
    VERSION 2.7.1
    LANGUAGES CXX)

INCLUDE(GNUInstallDirs)

OPTION( agg_USE_FREETYPE "Use Freetype library" ON)
OPTION( agg_USE_EXPAT "Use Expat library" OFF)
OPTION( agg_USE_SDL_PLATFORM "Use SDL as platform" OFF)
OPTION( agg_USE_PACK "Package Agg" OFF)
OPTION( agg_USE_AGG2D "Agg 2D graphical context" OFF)
OPTION( agg_USE_AGG2D_FREETYPE "Agg 2D graphical context uses freetype" OFF)
OPTION( agg_BUILD_EXAMPLES "Build the AGG examples" OFF)
OPTION( agg_BUILD_DEMO "Build the AGG demo" OFF)
OPTION( agg_BUILD_PLATFORM "Build the AGG platform helper" OFF)
OPTION( agg_BUILD_CONTROLS "Build the AGG controls" OFF)

IF ( UNIX )
    IF ( agg_USE_AGG2D )
        IF ( NOT agg_USE_AGG2D_FREETYPE )
            MESSAGE( FATAL_ERROR "agg_USE_AGG2D_FREETYPE must be enabled" )
        ENDIF( NOT agg_USE_AGG2D_FREETYPE )
    ENDIF( agg_USE_AGG2D )
ENDIF(UNIX)

IF ( agg_BUILD_DEMO OR agg_BUILD_EXAMPLES )
    IF ( NOT agg_BUILD_PLATFORM )
        MESSAGE( FATAL_ERROR "agg_BUILD_PLATFORM must be enabled" )
    ENDIF( NOT agg_BUILD_PLATFORM )
    IF ( NOT agg_BUILD_CONTROLS )
        MESSAGE( FATAL_ERROR "agg_BUILD_CONTROLS must be enabled" )
    ENDIF( NOT agg_BUILD_CONTROLS )
ENDIF( agg_BUILD_DEMO OR agg_BUILD_EXAMPLES )

SET (antigrain_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
SET (agg_VERSION ${PROJECT_VERSION})
# for the moment this decides the platform code.
IF(WIN32)
    ADD_DEFINITIONS( -D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE )
    SET( WIN32GUI WIN32 )    
ENDIF(WIN32)

IF(UNIX)
    SET( WIN32GUI "" )
ENDIF(UNIX)



ADD_SUBDIRECTORY( src )

IF ( agg_USE_FREETYPE )

    ADD_SUBDIRECTORY( font_freetype )

ENDIF ( agg_USE_FREETYPE )

IF ( agg_USE_AGG2D )

    ADD_SUBDIRECTORY( agg2d )

ENDIF ( agg_USE_AGG2D )


IF( agg_BUILD_PLATFORM )

    IF(WIN32)
        ADD_SUBDIRECTORY( font_win32_tt )
    ENDIF ( WIN32 )

ENDIF ( agg_BUILD_PLATFORM )

IF ( agg_BUILD_EXAMPLES )

    ADD_SUBDIRECTORY( examples )

ENDIF( agg_BUILD_EXAMPLES )




CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/agg-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    @ONLY
)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/agg-config-version.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    @ONLY
)
INSTALL(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}/
)


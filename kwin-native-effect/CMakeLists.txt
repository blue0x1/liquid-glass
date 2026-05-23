cmake_minimum_required(VERSION 3.24)
project(liquidglassnativeeffect VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(QT_NO_CREATE_VERSIONLESS_TARGETS ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(KF6CoreAddons REQUIRED CONFIG)

find_path(KWIN_EFFECT_INCLUDE_DIR
    NAMES kwin/effect/effect.h
    PATHS /usr/include
    REQUIRED
)

find_path(KF6CONFIGCORE_INCLUDE_DIR
    NAMES ksharedconfig.h
    PATHS /usr/include/KF6/KConfigCore
    REQUIRED
)

find_library(KWIN_LIBRARY
    NAMES kwin
    PATHS /usr/lib/x86_64-linux-gnu /usr/lib
    REQUIRED
)

find_library(KF6CONFIGCORE_LIBRARY
    NAMES KF6ConfigCore
    PATHS /usr/lib/x86_64-linux-gnu /usr/lib
    REQUIRED
)

find_library(KF6CONFIGGUI_LIBRARY
    NAMES KF6ConfigGui
    PATHS /usr/lib/x86_64-linux-gnu /usr/lib
    REQUIRED
)

find_library(EPOXY_LIBRARY
    NAMES epoxy
    PATHS /usr/lib/x86_64-linux-gnu /usr/lib
    REQUIRED
)

find_library(GL_LIBRARY
    NAMES GL
    PATHS /usr/lib/x86_64-linux-gnu /usr/lib
    REQUIRED
)

add_library(kwin_effect_liquidglassnative MODULE
    src/liquid_glass_native_effect.cpp
)

target_include_directories(kwin_effect_liquidglassnative PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${KWIN_EFFECT_INCLUDE_DIR}
    /usr/include/kwin
    /usr/include/KF6/KConfig
    /usr/include/KF6/KConfigCore
    /usr/include/KF6/KConfigGui
    ${KF6CONFIGCORE_INCLUDE_DIR}
)
target_link_libraries(kwin_effect_liquidglassnative
    PRIVATE
        Qt6::Core
        Qt6::Gui
        KF6::CoreAddons
        ${KF6CONFIGCORE_LIBRARY}
        ${KF6CONFIGGUI_LIBRARY}
        ${KWIN_LIBRARY}
        ${EPOXY_LIBRARY}
        ${GL_LIBRARY}
)

install(TARGETS kwin_effect_liquidglassnative DESTINATION ${CMAKE_INSTALL_LIBDIR}/qt6/plugins/kwin/effects/plugins)
install(FILES package/liquidglassnative.json DESTINATION ${KDE_INSTALL_DATADIR}/kwin/builtin-effects)
install(FILES shaders/liquidglass.frag DESTINATION ${KDE_INSTALL_DATADIR}/kwin/shaders/liquidglassnative)

cmake_minimum_required(VERSION 3.27)

project(MyQuant VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(APPLE AND NOT TARGET WrapOpenGL::WrapOpenGL)
    find_package(OpenGL QUIET)
    if(OpenGL_FOUND)
        add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED)
        if(TARGET OpenGL::GL)
            target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE OpenGL::GL)
        endif()
    endif()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql Network)

if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    foreach(_prop INTERFACE_LINK_LIBRARIES INTERFACE_LINK_OPTIONS)
        get_target_property(_gl_items WrapOpenGL::WrapOpenGL ${_prop})
        if(_gl_items)
            set(_clean_gl_items "")
            set(_skip_next OFF)
            foreach(_item IN LISTS _gl_items)
                if(_skip_next)
                    set(_skip_next OFF)
                    continue()
                endif()
                if(_item STREQUAL "-framework")
                    set(_skip_next ON)
                    continue()
                endif()
                if(NOT _item MATCHES "AGL")
                    list(APPEND _clean_gl_items "${_item}")
                endif()
            endforeach()
            set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES ${_prop} "${_clean_gl_items}")
        endif()
    endforeach()
endif()

qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MyQuantInfo.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MyQuantInfo.plist"
    @ONLY
)

set(MYQUANT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/MyQuant.icns")
if(EXISTS "${MYQUANT_ICON}")
    set_source_files_properties("${MYQUANT_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

qt_add_executable(MyQuant
    MACOSX_BUNDLE
    ${MYQUANT_ICON}
    src/main.cpp
    src/AppPaths.cpp
    src/AppPaths.h
    src/SettingsController.cpp
    src/SettingsController.h
    src/EtfRotationController.cpp
    src/EtfRotationController.h
    src/NotesController.cpp
    src/NotesController.h
)

qt_add_qml_module(MyQuant
    URI MyQuant
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/LineChart.qml
        qml/components/AppScrollBar.qml
        qml/components/MetricCard.qml
        qml/components/SectionPanel.qml
        qml/components/TradeChart.qml
        qml/pages/EtfPage.qml
        qml/pages/NotesPage.qml
        qml/pages/SettingsPage.qml
)

target_link_libraries(MyQuant
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Sql
        Qt6::Network
)

set_target_properties(MyQuant PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.qirunzeng.myquant"
    MACOSX_BUNDLE_BUNDLE_NAME "MyQuant"
    MACOSX_BUNDLE_ICON_FILE "MyQuant.icns"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright 2026 qirunzeng. All rights reserved."
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/MyQuantInfo.plist"
)

if(APPLE)
    set(_MYQUANT_RES_DST "$<TARGET_BUNDLE_DIR:MyQuant>/Contents/Resources/resources")
    set(_MYQUANT_PLUGIN_DST "$<TARGET_BUNDLE_DIR:MyQuant>/Contents/PlugIns")
else()
    set(_MYQUANT_RES_DST "$<TARGET_FILE_DIR:MyQuant>/resources")
    set(_MYQUANT_PLUGIN_DST "$<TARGET_FILE_DIR:MyQuant>")
endif()
get_filename_component(_MYQUANT_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)

add_custom_command(TARGET MyQuant POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "${_MYQUANT_RES_DST}"
    COMMENT "Copying MyQuant runtime resources"
)

if(APPLE)
    add_custom_command(TARGET MyQuant POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/MyQuantInfo.plist"
            "$<TARGET_BUNDLE_DIR:MyQuant>/Contents/Info.plist"
        COMMENT "Writing MyQuant bundle Info.plist"
    )
    if(EXISTS "${MYQUANT_ICON}")
        add_custom_command(TARGET MyQuant POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MYQUANT_ICON}"
                "$<TARGET_BUNDLE_DIR:MyQuant>/Contents/Resources/MyQuant.icns"
            COMMENT "Copying MyQuant app icon"
        )
    endif()
    if(EXISTS "${_MYQUANT_QT_PREFIX}/bin/macdeployqt")
        add_custom_command(TARGET MyQuant POST_BUILD
            COMMAND "${_MYQUANT_QT_PREFIX}/bin/macdeployqt"
                "$<TARGET_BUNDLE_DIR:MyQuant>"
                -qmldir="${CMAKE_CURRENT_SOURCE_DIR}/qml"
                -always-overwrite
                -no-plugins
                -no-strip
            COMMENT "Deploying Qt frameworks and QML modules"
        )
    endif()
    foreach(_plugin_dir platforms imageformats tls)
        if(EXISTS "${_MYQUANT_QT_PREFIX}/plugins/${_plugin_dir}")
            add_custom_command(TARGET MyQuant POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_MYQUANT_QT_PREFIX}/plugins/${_plugin_dir}"
                    "${_MYQUANT_PLUGIN_DST}/${_plugin_dir}"
                COMMENT "Copying Qt ${_plugin_dir} plugins"
            )
        endif()
    endforeach()
    if(EXISTS "${_MYQUANT_QT_PREFIX}/plugins/sqldrivers/libqsqlite.dylib")
        add_custom_command(TARGET MyQuant POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_MYQUANT_PLUGIN_DST}/sqldrivers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_MYQUANT_QT_PREFIX}/plugins/sqldrivers/libqsqlite.dylib"
                "${_MYQUANT_PLUGIN_DST}/sqldrivers/libqsqlite.dylib"
            COMMENT "Copying Qt SQLite plugin"
        )
    endif()
    set(_MYQUANT_QML_DST "$<TARGET_BUNDLE_DIR:MyQuant>/Contents/Resources/qml")
    foreach(_qml_dir QtCore QtQml QtQuick)
        if(EXISTS "${_MYQUANT_QT_PREFIX}/qml/${_qml_dir}")
            add_custom_command(TARGET MyQuant POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_MYQUANT_QT_PREFIX}/qml/${_qml_dir}"
                    "${_MYQUANT_QML_DST}/${_qml_dir}"
                COMMENT "Copying Qt QML module ${_qml_dir}"
            )
        endif()
    endforeach()
    file(GLOB _MYQUANT_EXTRA_QT_FRAMEWORKS CONFIGURE_DEPENDS
        "${_MYQUANT_QT_PREFIX}/lib/QtQuickControls2*.framework"
        "${_MYQUANT_QT_PREFIX}/lib/QtQuickDialogs2*.framework"
    )
    foreach(_framework_path IN LISTS _MYQUANT_EXTRA_QT_FRAMEWORKS)
        if(IS_DIRECTORY "${_framework_path}")
            get_filename_component(_framework "${_framework_path}" NAME)
            add_custom_command(TARGET MyQuant POST_BUILD
                COMMAND /usr/bin/ditto --norsrc
                    "${_framework_path}"
                    "$<TARGET_BUNDLE_DIR:MyQuant>/Contents/Frameworks/${_framework}"
                COMMENT "Copying Qt framework ${_framework}"
            )
        endif()
    endforeach()
    foreach(_framework
            QtQmlCore
            QtQmlLocalStorage
            QtQmlXmlListModel
            QtQuickEffects
            QtQuickLayouts
            QtQuickParticles
            QtQuickShapes
            QtQuickVectorImage
            QtSvg)
        if(EXISTS "${_MYQUANT_QT_PREFIX}/lib/${_framework}.framework")
            add_custom_command(TARGET MyQuant POST_BUILD
                COMMAND /usr/bin/ditto --norsrc
                    "${_MYQUANT_QT_PREFIX}/lib/${_framework}.framework"
                    "$<TARGET_BUNDLE_DIR:MyQuant>/Contents/Frameworks/${_framework}.framework"
                COMMENT "Copying Qt framework ${_framework}"
            )
        endif()
    endforeach()
    add_custom_command(TARGET MyQuant POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -D "APP_BUNDLE=$<TARGET_BUNDLE_DIR:MyQuant>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fix_macos_bundle.cmake"
        COMMENT "Fixing and signing MyQuant macOS bundle"
    )
endif()

enable_testing()
add_executable(myquant_smoke_tests
    tests/smoke_tests.cpp
    src/AppPaths.cpp
    src/SettingsController.cpp
    src/EtfRotationController.cpp
    src/NotesController.cpp
)
target_link_libraries(myquant_smoke_tests
    PRIVATE Qt6::Core Qt6::Gui Qt6::Sql Qt6::Network
)
add_test(NAME myquant_smoke_tests COMMAND myquant_smoke_tests)

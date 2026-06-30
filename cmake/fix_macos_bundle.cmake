if(NOT DEFINED APP_BUNDLE)
    message(FATAL_ERROR "APP_BUNDLE is required")
endif()

set(_plugin_root "${APP_BUNDLE}/Contents/PlugIns")
if(IS_DIRECTORY "${_plugin_root}")
    file(GLOB_RECURSE _plugins "${_plugin_root}/*.dylib")
    foreach(_plugin IN LISTS _plugins)
        execute_process(
            COMMAND install_name_tool -add_rpath "@loader_path/../../Frameworks" "${_plugin}"
            RESULT_VARIABLE _rpath_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
    endforeach()
endif()

set(_qml_root "${APP_BUNDLE}/Contents/Resources/qml")
if(IS_DIRECTORY "${_qml_root}")
    file(GLOB_RECURSE _qml_plugins "${_qml_root}/*.dylib")
    foreach(_plugin IN LISTS _qml_plugins)
        foreach(_rpath
                "@loader_path/../../../../Frameworks"
                "@loader_path/../../../../../Frameworks"
                "@loader_path/../../../../../../Frameworks")
            execute_process(
                COMMAND install_name_tool -add_rpath "${_rpath}" "${_plugin}"
                RESULT_VARIABLE _qml_rpath_result
                OUTPUT_QUIET
                ERROR_QUIET
            )
        endforeach()
    endforeach()
endif()

execute_process(
    COMMAND find "${APP_BUNDLE}" -name "*.dSYM" -type d -prune -exec rm -rf "{}" "+"
    OUTPUT_QUIET
    ERROR_QUIET
)
execute_process(
    COMMAND find "${APP_BUNDLE}" -name ".DS_Store" -type f -delete
    OUTPUT_QUIET
    ERROR_QUIET
)
execute_process(COMMAND dot_clean -m "${APP_BUNDLE}" OUTPUT_QUIET ERROR_QUIET)
execute_process(COMMAND xattr -cr "${APP_BUNDLE}" OUTPUT_QUIET ERROR_QUIET)

file(GLOB_RECURSE _nested_plugins
    "${APP_BUNDLE}/Contents/PlugIns/*.dylib"
    "${APP_BUNDLE}/Contents/Resources/qml/*.dylib"
)
foreach(_binary IN LISTS _nested_plugins)
    execute_process(COMMAND codesign --force --sign - "${_binary}" OUTPUT_QUIET ERROR_QUIET)
endforeach()

file(GLOB _framework_versions LIST_DIRECTORIES true
    "${APP_BUNDLE}/Contents/Frameworks/*.framework/Versions/A"
)
foreach(_framework IN LISTS _framework_versions)
    execute_process(COMMAND codesign --force --sign - "${_framework}" OUTPUT_QUIET ERROR_QUIET)
endforeach()

execute_process(COMMAND codesign --force --sign - "${APP_BUNDLE}/Contents/MacOS/MyQuant" OUTPUT_QUIET ERROR_QUIET)
execute_process(COMMAND codesign --force --sign - "${APP_BUNDLE}" OUTPUT_QUIET ERROR_QUIET)

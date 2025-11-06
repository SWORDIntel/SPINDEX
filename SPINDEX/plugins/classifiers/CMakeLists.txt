# --- Classifier Plugins CMakeLists.txt ---

macro(add_classifier_plugin plugin_id source_file)
    set(plugin_target_name "${plugin_id}_classifier_so")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
        add_library(${plugin_target_name} MODULE ${source_file})
        set_target_properties(${plugin_target_name} PROPERTIES PREFIX "")
        set_target_properties(${plugin_target_name} PROPERTIES OUTPUT_NAME "${plugin_id}_classifier")

        target_include_directories(${plugin_target_name} PUBLIC
            "${CMAKE_SOURCE_DIR}/include"
        )

        if("${plugin_id}" STREQUAL "openvino")
            if(OPENVINO_FOUND) # This variable is set by find_package(OpenVINO CONFIG) in root CMake
                # Assumes OpenVINO::OpenVINO or similar target is provided by Conan's OpenVINO recipe
                # This target should handle include directories.
                target_link_libraries(${plugin_target_name} PRIVATE spindex_core_lib OpenVINO::OpenVINO)
                # RPATH settings are typically handled by Conan toolchain or OpenVINO's recipe.
                # If specific RPATH is still needed for the plugin, it could be added here,
                # but it's better if Conan's setup makes it unnecessary.
            else()
                message(WARNING "OpenVINO not found/configured by Conan. ${plugin_target_name} will not be built correctly or skipped.")
                # Depending on how add_classifier_plugin is called, this plugin might not be added if OPENVINO_FOUND is false.
            endif()
        endif()

        list(APPEND SPINDEX_ALL_PLUGIN_TARGETS ${plugin_target_name})
        set(SPINDEX_ALL_PLUGIN_TARGETS ${SPINDEX_ALL_PLUGIN_TARGETS} CACHE INTERNAL "List of all plugin targets" FORCE)
        message(STATUS "Configured classifier plugin: ${plugin_target_name} (${plugin_id}_classifier.so)")
    else()
        message(WARNING "Source file ${source_file} not found for ${plugin_id}_classifier.")
    endif()
endmacro()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/openvino_classifier.c")
    if(OPENVINO_FOUND) # Only attempt to build if OpenVINO was found by root CMake
        add_classifier_plugin(openvino openvino_classifier.c)
    else()
        message(STATUS "OpenVINO not found, OpenVINO classifier plugin will be skipped.")
    endif()
else()
    message(STATUS "openvino_classifier.c not found, skipping OpenVINO classifier plugin.")
endif()

message(STATUS "Classifier plugins CMakeLists.txt processing complete.")

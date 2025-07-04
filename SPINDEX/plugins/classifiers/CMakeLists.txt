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
        target_compile_options(${plugin_target_name} PRIVATE ${CMAKE_C_FLAGS})

        if("${plugin_id}" STREQUAL "openvino")
            if(OPENVINO_FOUND)
                # OpenVINO_C_INCLUDE_DIR is already globally included or part of spindex_core_lib's interface includes.
                # If openvino_classifier.c directly includes OpenVINO headers, this is fine.
                # target_include_directories(${plugin_target_name} PRIVATE ${OpenVINO_C_INCLUDE_DIR}) # Usually not needed if core lib propagates

                target_link_libraries(${plugin_target_name} PRIVATE spindex_core_lib) # For utils.o etc.
                target_link_libraries(${plugin_target_name} PRIVATE ${OpenVINO_C_LIBRARIES}) # Link OpenVINO libs

                if(OpenVINO_C_LIBRARY_DIR AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
                    target_link_options(${plugin_target_name} PRIVATE "-Wl,-rpath,${OpenVINO_C_LIBRARY_DIR}")
                endif()
            else()
                message(WARNING "OpenVINO not found/configured. ${plugin_target_name} will not be built correctly.")
                # Optionally, don't even add the library if OpenVINO is critical and not found.
                # return() # from macro
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

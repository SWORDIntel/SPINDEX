# --- Top-Level/Integration Tests CMakeLists.txt ---

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_openvino_classifier_hw.c")
    option(BUILD_TEST_OPENVINO_HW "Build the OpenVINO hardware-specific test" ON)
    if(BUILD_TEST_OPENVINO_HW)
        if(OPENVINO_FOUND) # Only build if OpenVINO is configured
            add_executable(test_openvino_classifier_hw test_openvino_classifier_hw.c)
            target_compile_options(test_openvino_classifier_hw PRIVATE ${CMAKE_C_FLAGS})
            target_include_directories(test_openvino_classifier_hw PRIVATE
                "${CMAKE_SOURCE_DIR}/include" # For plugin_interface.h, data_types.h etc.
            )

            # This test dynamically loads the OpenVINO classifier plugin.
            # It needs OpenVINO libraries for its own code if it calls OpenVINO APIs directly,
            # and dl libs for loading.
            target_link_libraries(test_openvino_classifier_hw PRIVATE
                ${CMAKE_DL_LIBS} # For dlopen
                ${OpenVINO_C_LIBRARIES} # Link against OpenVINO libs (e.g. openvino_c)
            )

            if(OpenVINO_C_LIBRARY_DIR AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
                target_link_options(test_openvino_classifier_hw PRIVATE "-Wl,-rpath,${OpenVINO_C_LIBRARY_DIR}")
            endif()

            # Ensure the test knows where to find the openvino_classifier.so plugin
            set_tests_properties(test_openvino_classifier_hw PROPERTIES
                ENVIRONMENT "SPINDEX_PLUGIN_DIR=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

            # Make this test depend on the OpenVINO classifier plugin if it's being built
            if(TARGET openvino_classifier_so)
                 add_dependencies(test_openvino_classifier_hw openvino_classifier_so)
            else()
                # If the specific plugin target isn't easily accessible here,
                # depend on the general plugins_built target.
                add_dependencies(test_openvino_classifier_hw plugins_built)
                message(STATUS "test_openvino_classifier_hw will depend on the 'plugins_built' custom target. Ensure openvino_classifier_so is part of it.")
            endif()

            add_test(NAME test_openvino_classifier_hw COMMAND test_openvino_classifier_hw)
            message(STATUS "Added OpenVINO Hardware test: test_openvino_classifier_hw")
        else()
            message(STATUS "OpenVINO not found/configured, skipping test_openvino_classifier_hw build.")
        endif()
    else()
        message(STATUS "BUILD_TEST_OPENVINO_HW is OFF, skipping test_openvino_classifier_hw.")
    endif()
else()
    message(STATUS "test_openvino_classifier_hw.c not found in tests directory. Skipping this test.")
endif()

message(STATUS "SPINDEX/tests/CMakeLists.txt processing complete.")

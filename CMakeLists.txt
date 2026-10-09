cmake_minimum_required(VERSION 3.24)
project(crash_bash_port LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CTest)

# PSXPORT_DIR: the framework; external/psxport is a symlink to the live checkout or a clone of main
# (tools/psxport_fetch.py --auto).
set(PSXPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory (default: external/psxport)")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake — run "
                      "`uv run --frozen python tools/psxport_fetch.py --auto`, or point PSXPORT_DIR "
                      "at a psxport checkout.")
endif()
set(PSXPORT_BUILD_SMOKE OFF CACHE BOOL "Build the framework agnosticism smoke" FORCE)
add_subdirectory("${PSXPORT_DIR}" psxport_build)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS titles/crashbash/executable.json)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/crashbash/executable.json" CRASHBASH_EXECUTABLE_MANIFEST)
string(JSON CRASHBASH_EXECUTABLE_SHA256 GET "${CRASHBASH_EXECUTABLE_MANIFEST}" sha256)
string(JSON CRASHBASH_EXECUTABLE_SIZE GET "${CRASHBASH_EXECUTABLE_MANIFEST}" file_size)
string(JSON CRASHBASH_EXECUTABLE_ENTRY GET "${CRASHBASH_EXECUTABLE_MANIFEST}" header entry)
string(JSON CRASHBASH_EXECUTABLE_TEXT GET "${CRASHBASH_EXECUTABLE_MANIFEST}" header text_address)
string(JSON CRASHBASH_EXECUTABLE_TEXT_SIZE GET "${CRASHBASH_EXECUTABLE_MANIFEST}" header text_size)
configure_file(game/execution/executable_identity.h.in title_metadata/executable_identity.h @ONLY)

function(configure_crashbash_module_identity module_name)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "titles/crashbash/${module_name}_module.json")
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/crashbash/${module_name}_module.json" MODULE_MANIFEST)
  string(JSON MODULE_DISC_LBA GET "${MODULE_MANIFEST}" payload_disc_lba)
  string(JSON MODULE_SECTOR_COUNT GET "${MODULE_MANIFEST}" sector_count)
  string(JSON MODULE_PAYLOAD_BYTES GET "${MODULE_MANIFEST}" payload_size)
  string(JSON MODULE_SHA256 GET "${MODULE_MANIFEST}" payload_sha256)
  string(JSON MODULE_LOAD_ADDRESS GET "${MODULE_MANIFEST}" load_address)
  # The nested overlays record `entry_pointer_offset` and `entry` as JSON null; GET errors on null, so test the type first.
  string(JSON MODULE_ENTRY_OFFSET_TYPE TYPE "${MODULE_MANIFEST}" entry_pointer_offset)
  string(JSON MODULE_ENTRY_TYPE TYPE "${MODULE_MANIFEST}" entry)
  if(NOT MODULE_ENTRY_OFFSET_TYPE STREQUAL "NULL" AND NOT MODULE_ENTRY_TYPE STREQUAL "NULL")
    string(JSON MODULE_ENTRY_OFFSET GET "${MODULE_MANIFEST}" entry_pointer_offset)
    string(JSON MODULE_ENTRY GET "${MODULE_MANIFEST}" entry)
  else()
    set(MODULE_ENTRY_OFFSET 0)
    set(MODULE_ENTRY 0)
  endif()
  set(MODULE_NAME "${module_name}")
  configure_file(game/execution/module_image_identity.h.in "title_metadata/${module_name}_image_identity.h" @ONLY)
endfunction()

configure_crashbash_module_identity(boot)
configure_crashbash_module_identity(menu)
configure_crashbash_module_identity(dat28136)
configure_crashbash_module_identity(dat28272)
configure_crashbash_module_identity(dat28241)
configure_crashbash_module_identity(dat28382)
configure_crashbash_module_identity(dat22510)

set(CRASHBASH_TITLE_INCLUDE_DIRS
  game/boot
  game/debug
  game/disc
  game/entry
  game/execution
  game/frame
  game/gameplay
  game/input
  game/render
  game/title
)

set(CRASHBASH_SEAM_SOURCES
  game/boot/boot_object_callbacks.cpp
  game/boot/boot_logo_skip.cpp
  game/boot/authenticated_module_image.cpp
  game/boot/nested_module_image.cpp
  game/boot/boot_image.cpp
  game/disc/cd_file_read.cpp
  game/disc/loading_card_skip.cpp
  game/disc/cd_license_startup.cpp
  game/disc/cd_startup.cpp
  game/boot/crashbash_boot.cpp
  game/frame/crashbash_frame_driver.cpp
  game/frame/frame_cut.cpp
  game/input/crashbash_input_phase.cpp
  game/debug/dev_arena.cpp
  game/debug/dev_pause.cpp
  game/frame/display_frame.cpp
  game/frame/gpu_timeout.cpp
  game/boot/memory_card_startup.cpp
  game/boot/menu_image.cpp
  game/execution/native_owner_set.cpp
  game/execution/title_adapter.cpp
  game/boot/resident_image.cpp
  game/gameplay/polar_push_contact.cpp
  game/input/crashbash_touch_controls.cpp
  game/render/model_face_producer.cpp
  game/render/component_incarnation.cpp
  game/render/ui_producer.cpp
  game/render/leaf_port.cpp
  game/render/host_leaf.cpp
  game/render/text_bodies.cpp
  game/render/panel_bodies.cpp
  game/render/component_body.cpp
  game/render/ordering_table_slots.cpp
  game/render/draw_globals.cpp
  game/render/packet_decode.cpp
  game/render/face_projection.cpp
  game/render/leaf_packets.cpp
  game/render/model_state.cpp
  game/render/leaf_state.cpp
  game/render/packet_collector.cpp
  game/render/state_renders.cpp
)

find_package(SDL3 REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(
  crashbash_appimage_launcher
  packaging/linux/launcher_main.cpp
  packaging/linux/install_media.cpp
  packaging/linux/user_paths.cpp
  "${PSXPORT_DIR}/vendor/lucent/src/zip.cpp"
)
target_include_directories(
  crashbash_appimage_launcher
  PRIVATE
    packaging/linux
    "${PSXPORT_DIR}/vendor/lucent/include"
)
target_link_libraries(crashbash_appimage_launcher PRIVATE lucent SDL3::SDL3 ZLIB::ZLIB)
set_target_properties(
  crashbash_appimage_launcher
  PROPERTIES
    OUTPUT_NAME crashbash-launcher
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_library(crashbash_guest_execution STATIC game/execution/guest_execution.cpp
                                            game/execution/image_identity_state.cpp)
set_target_properties(crashbash_guest_execution PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(crashbash_guest_execution PUBLIC game/execution game/boot game/title game/disc)
target_link_libraries(crashbash_guest_execution PUBLIC psxport)

add_library(crashbash_seam OBJECT ${CRASHBASH_SEAM_SOURCES})
set_target_properties(crashbash_seam PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(crashbash_seam PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS} "${CMAKE_CURRENT_BINARY_DIR}/title_metadata")
target_link_libraries(crashbash_seam PUBLIC crashbash_guest_execution)
add_custom_target(crashbash_scaffold DEPENDS crashbash_seam psxport)

add_executable(crashbash_port game/entry/player_entry.cpp $<TARGET_OBJECTS:crashbash_seam>)
target_include_directories(crashbash_port PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_BINARY_DIR}/title_metadata")
target_link_libraries(crashbash_port PRIVATE crashbash_guest_execution psxport)
set_target_properties(
  crashbash_port
  PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(BUILD_TESTING)
  add_executable(crashbash_title_adapter_test tests/test_title_adapter.cpp)
  target_include_directories(crashbash_title_adapter_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS} "${PSXPORT_DIR}/tests" "${CMAKE_CURRENT_BINARY_DIR}/title_metadata")
  target_link_libraries(crashbash_title_adapter_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_title_adapter_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_title_adapter COMMAND crashbash_title_adapter_test)
  set_tests_properties(crashbash_title_adapter PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "PSXPORT_CARD=${CMAKE_CURRENT_SOURCE_DIR}/scratch/title-adapter-test/card.mcr")
  add_executable(crashbash_input_phase_test tests/test_input_phase.cpp)
  target_include_directories(crashbash_input_phase_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS} "${PSXPORT_DIR}/tests")
  target_link_libraries(crashbash_input_phase_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_input_phase_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_input_phase COMMAND crashbash_input_phase_test)
  add_executable(crashbash_boot_image_test tests/test_boot_image.cpp)
  target_include_directories(crashbash_boot_image_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS}
                           "${PSXPORT_DIR}/tests"
                           "${CMAKE_CURRENT_BINARY_DIR}/title_metadata")
  target_link_libraries(crashbash_boot_image_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_boot_image_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_boot_image COMMAND crashbash_boot_image_test)
  add_executable(crashbash_menu_image_test tests/test_menu_image.cpp)
  target_include_directories(crashbash_menu_image_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS}
                           "${PSXPORT_DIR}/tests"
                           "${CMAKE_CURRENT_BINARY_DIR}/title_metadata")
  target_link_libraries(crashbash_menu_image_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_menu_image_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_menu_image COMMAND crashbash_menu_image_test)
  add_executable(crashbash_nested_module_image_test tests/test_nested_module_image.cpp)
  target_include_directories(crashbash_nested_module_image_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS}
                                                                    "${PSXPORT_DIR}/tests"
                                                                    "${CMAKE_CURRENT_BINARY_DIR}/title_metadata")
  target_link_libraries(crashbash_nested_module_image_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_nested_module_image_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_nested_module_image COMMAND crashbash_nested_module_image_test)
  add_executable(crashbash_guest_execution_test tests/test_guest_execution.cpp)
  target_include_directories(crashbash_guest_execution_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS} "${PSXPORT_DIR}/tests")
  target_link_libraries(crashbash_guest_execution_test PRIVATE crashbash_guest_execution)
  set_target_properties(crashbash_guest_execution_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_guest_execution COMMAND crashbash_guest_execution_test)
  add_executable(
    crashbash_touch_controls_test
    tests/test_crashbash_touch_controls.cpp
    game/input/crashbash_touch_controls.cpp
  )
  target_include_directories(crashbash_touch_controls_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS})
  set_target_properties(
    crashbash_touch_controls_test
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
  )
  add_test(NAME crashbash_touch_controls COMMAND crashbash_touch_controls_test)
  add_executable(crashbash_render_capabilities_test tests/test_render_capabilities.cpp)
  target_include_directories(crashbash_render_capabilities_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS} "${PSXPORT_DIR}/tests")
  target_link_libraries(crashbash_render_capabilities_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_render_capabilities_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_render_capabilities COMMAND crashbash_render_capabilities_test)
  add_executable(crashbash_dev_pause_test tests/test_dev_pause.cpp)
  target_include_directories(crashbash_dev_pause_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS} "${PSXPORT_DIR}/tests")
  target_link_libraries(crashbash_dev_pause_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_dev_pause_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_dev_pause COMMAND crashbash_dev_pause_test)
  add_executable(crashbash_frame_cut_test tests/test_frame_cut.cpp)
  target_include_directories(crashbash_frame_cut_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS} "${PSXPORT_DIR}/tests")
  target_link_libraries(crashbash_frame_cut_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_frame_cut_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_frame_cut COMMAND crashbash_frame_cut_test)
  add_executable(crashbash_draw_base_test tests/test_draw_base.cpp)
  target_include_directories(crashbash_draw_base_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS} "${PSXPORT_DIR}/tests")
  target_link_libraries(crashbash_draw_base_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_draw_base_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_draw_base COMMAND crashbash_draw_base_test)
  add_executable(crashbash_model_face_producer_test tests/test_model_face_producer.cpp)
  target_include_directories(crashbash_model_face_producer_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS} "${PSXPORT_DIR}/tests")
  target_link_libraries(crashbash_model_face_producer_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_model_face_producer_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_model_face_producer COMMAND crashbash_model_face_producer_test)
  add_executable(crashbash_ui_producer_test tests/test_ui_producer.cpp)
  target_include_directories(crashbash_ui_producer_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS} "${PSXPORT_DIR}/tests")
  target_link_libraries(crashbash_ui_producer_test PRIVATE crashbash_seam)
  set_target_properties(crashbash_ui_producer_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME crashbash_ui_producer COMMAND crashbash_ui_producer_test)
  # Reads its census through `frameDriver(core)`; the seam supplies the owner.
  add_executable(crashbash_polar_push_contact_test tests/test_polar_push_contact.cpp)
  target_include_directories(crashbash_polar_push_contact_test PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS})
  target_link_libraries(crashbash_polar_push_contact_test PRIVATE crashbash_seam)
  set_target_properties(
    crashbash_polar_push_contact_test
    PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
  )
  add_test(NAME crashbash_polar_push_contact COMMAND crashbash_polar_push_contact_test)
  add_test(
    NAME crashbash_native_ownership_selftest
    COMMAND
      "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_source_policy.py"
  )
  
  add_test(
    NAME crashbash_cpp_policy
    COMMAND
      "${Python3_EXECUTABLE}" "${PSXPORT_DIR}/tools/check_cpp_style.py"
      --root "${CMAKE_CURRENT_SOURCE_DIR}" --compile-commands "${CMAKE_BINARY_DIR}"
  )
  add_test(
    NAME crashbash_bootstrap_selftest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bootstrap.py"
  )
  add_test(
    NAME crashbash_provision_selftest
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_provision.py"
  )
endif()

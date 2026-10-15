cmake_minimum_required(VERSION 3.24)
project(crash_bash_port LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(CTest)

# PSXPORT_DIR: the framework; external/psxport is a symlink to the live checkout or a clone of main
# (psxport tools/psxport_fetch.py).
set(PSXPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory (default: external/psxport)")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake — run "
                      "./run.sh once to fetch it, or point PSXPORT_DIR at a psxport checkout.")
endif()
set(PSXPORT_BUILD_SMOKE OFF CACHE BOOL "Build the framework agnosticism smoke" FORCE)
add_subdirectory("${PSXPORT_DIR}" psxport_build)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS titles/crashbash/executable.json)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/titles/crashbash/executable.json" CRASHBASH_EXECUTABLE_MANIFEST)
string(JSON CRASHBASH_EXECUTABLE_SHA256 GET "${CRASHBASH_EXECUTABLE_MANIFEST}" sha256)
string(JSON CRASHBASH_EXECUTABLE_SIZE GET "${CRASHBASH_EXECUTABLE_MANIFEST}" file_size)
string(JSON CRASHBASH_EXECUTABLE_ENTRY GET "${CRASHBASH_EXECUTABLE_MANIFEST}" header entry)
string(JSON CRASHBASH_EXECUTABLE_GP GET "${CRASHBASH_EXECUTABLE_MANIFEST}" header gp)
string(JSON CRASHBASH_EXECUTABLE_TEXT GET "${CRASHBASH_EXECUTABLE_MANIFEST}" header text_address)
string(JSON CRASHBASH_EXECUTABLE_TEXT_SIZE GET "${CRASHBASH_EXECUTABLE_MANIFEST}" header text_size)
string(JSON CRASHBASH_EXECUTABLE_STACK GET "${CRASHBASH_EXECUTABLE_MANIFEST}" header stack_address)
string(JSON CRASHBASH_EXECUTABLE_STACK_OFFSET GET "${CRASHBASH_EXECUTABLE_MANIFEST}" header stack_offset)
configure_file(game/execution/executable_identity.h.in title_metadata/execution/executable_identity.h @ONLY)

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
  configure_file(game/execution/module_image_identity.h.in "title_metadata/execution/${module_name}_image_identity.h" @ONLY)
endfunction()

configure_crashbash_module_identity(boot)
configure_crashbash_module_identity(menu)
configure_crashbash_module_identity(dat28136)
configure_crashbash_module_identity(dat28272)
configure_crashbash_module_identity(dat28241)
configure_crashbash_module_identity(dat28382)
configure_crashbash_module_identity(dat22510)

set(CRASHBASH_TITLE_INCLUDE_DIRS game)

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
  game/boot/boot_prefix.cpp
  game/frame/frame_driver.cpp
  game/frame/frame_cut.cpp
  game/input/recording_phase.cpp
  game/debug/dev_arena.cpp
  game/debug/dev_pause.cpp
  game/frame/display_frame.cpp
  game/frame/gpu_timeout.cpp
  game/boot/memory_card_startup.cpp
  game/boot/menu_image.cpp
  game/runtime/runtime.cpp
  game/runtime/session.cpp
  game/execution/guest_execution.cpp
  game/execution/image_identity_state.cpp
  game/gameplay/polar_push_contact.cpp
  game/input/touch_controls.cpp
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

add_library(crashbash_seam STATIC ${CRASHBASH_SEAM_SOURCES})
set_target_properties(crashbash_seam PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(crashbash_seam PUBLIC ${CRASHBASH_TITLE_INCLUDE_DIRS} "${CMAKE_CURRENT_BINARY_DIR}/title_metadata")
target_link_libraries(crashbash_seam PUBLIC psxport)

psxport_add_product(
  crashbash_port
  CATALOG crashbash::createCatalog
  HEADER entry/catalog.h
  SOURCES game/entry/catalog.cpp
  LIBRARIES crashbash_seam
)
target_include_directories(crashbash_port PRIVATE ${CRASHBASH_TITLE_INCLUDE_DIRS}
                           "${CMAKE_CURRENT_BINARY_DIR}/title_metadata")
set_target_properties(
  crashbash_port
  PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(BUILD_TESTING)
  add_test(NAME crashbash_product_help COMMAND $<TARGET_FILE:crashbash_port> --help)
  set_tests_properties(crashbash_product_help PROPERTIES PASS_REGULAR_EXPRESSION "Crash Bash \\(SCUS_945\\.70\\)")
  # One unit test over the title's shipping seam.
  function(crashbash_add_test name source)
    add_executable(crashbash_${name}_test ${source})
    target_include_directories(crashbash_${name}_test PRIVATE "${PSXPORT_DIR}/tests")
    target_link_libraries(crashbash_${name}_test PRIVATE crashbash_seam)
    set_target_properties(crashbash_${name}_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    add_test(NAME crashbash_${name} COMMAND crashbash_${name}_test)
  endfunction()
  crashbash_add_test(runtime tests/test_runtime.cpp)
  set_tests_properties(crashbash_runtime PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "PSXPORT_CARD=${CMAKE_CURRENT_SOURCE_DIR}/scratch/runtime-test/card.mcr")
  crashbash_add_test(recording_phase tests/test_recording_phase.cpp)
  crashbash_add_test(boot_image tests/test_boot_image.cpp)
  crashbash_add_test(menu_image tests/test_menu_image.cpp)
  crashbash_add_test(nested_module_image tests/test_nested_module_image.cpp)
  crashbash_add_test(guest_execution tests/test_guest_execution.cpp)
  target_link_libraries(crashbash_guest_execution_test PRIVATE psxport_test_support)
  crashbash_add_test(touch_controls tests/test_touch_controls.cpp)
  crashbash_add_test(record_widescreen tests/test_record_widescreen.cpp)
  crashbash_add_test(dev_pause tests/test_dev_pause.cpp)
  crashbash_add_test(frame_cut tests/test_frame_cut.cpp)
  crashbash_add_test(draw_base tests/test_draw_base.cpp)
  crashbash_add_test(model_face_producer tests/test_model_face_producer.cpp)
  crashbash_add_test(ui_producer tests/test_ui_producer.cpp)
  crashbash_add_test(polar_push_contact tests/test_polar_push_contact.cpp)
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

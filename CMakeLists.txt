cmake_minimum_required(VERSION 3.24)
project(crash_bash_port LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# PSXPORT_DIR: the framework; external/psxport is a symlink to the live checkout or a clone of main
# (psxport tools/psxport_fetch.py).
set(PSXPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory (default: external/psxport)")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake — run "
                      "./run.sh once to fetch it, or point PSXPORT_DIR at a psxport checkout.")
endif()
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
  game/debug/debug_menu.cpp
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

cmake_minimum_required(VERSION 3.20)
project(EddieDrawer VERSION 1.0 LANGUAGES CXX)
find_package(OpenCASCADE REQUIRED)
# RapidJSON is header-only; configuration is parsed locally at generation time.
find_path(RAPIDJSON_INCLUDE_DIR rapidjson/document.h REQUIRED)
add_executable(drawer src/main.cpp)
target_compile_features(drawer PRIVATE cxx_std_17)
target_include_directories(drawer SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIR})
target_link_libraries(drawer PRIVATE TKernel TKMath TKBRep TKPrim TKBO TKTopAlgo TKXSBase TKDESTEP TKXCAF TKLCAF TKCDF)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(drawer PRIVATE -Wall -Wextra -Wpedantic)
endif()
include(CTest)
if(BUILD_TESTING)
  find_program(JSONNET_EXECUTABLE NAMES jsonnet REQUIRED)
  file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/designs/*.libsonnet")
  set(EXPANDED_PROJECT "${CMAKE_CURRENT_BINARY_DIR}/default-project.json")
  add_custom_command(OUTPUT "${EXPANDED_PROJECT}"
    COMMAND ${CMAKE_COMMAND} "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/designs/cabinet.jsonnet"
      "-DOUTPUT=${EXPANDED_PROJECT}" "-DJSONNET_EXECUTABLE=${JSONNET_EXECUTABLE}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExpandProject.cmake"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/designs/cabinet.jsonnet" ${PROJECT_SOURCES}
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ExpandProject.cmake"
    VERBATIM)
  add_custom_target(project_config ALL DEPENDS "${EXPANDED_PROJECT}")
  add_executable(project_tests tests/project.cpp)
  add_dependencies(project_tests project_config)
  target_compile_features(project_tests PRIVATE cxx_std_17)
  target_include_directories(project_tests PRIVATE src)
  target_include_directories(project_tests SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIR})
  target_link_libraries(project_tests PRIVATE TKernel TKMath TKBRep TKPrim TKBO TKTopAlgo TKXSBase TKDESTEP TKXCAF TKLCAF TKCDF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(project_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME project_relationships COMMAND project_tests "${EXPANDED_PROJECT}" "${CMAKE_CURRENT_BINARY_DIR}/project-tests")
  add_test(NAME project_geometry_equivalence COMMAND ${CMAKE_COMMAND}
    "-DDRAWER=$<TARGET_FILE:drawer>" "-DCOMPARE=$<TARGET_FILE:project_tests>"
    "-DPROJECT=${EXPANDED_PROJECT}" "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/pre-config"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/project-equivalence"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/project-equivalence.cmake")
  add_test(NAME jsonnet_templates COMMAND ${CMAKE_COMMAND}
    "-DJSONNET_EXECUTABLE=${JSONNET_EXECUTABLE}" "-DDRAWER=$<TARGET_FILE:drawer>"
    "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/jsonnet-tests"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/jsonnet.cmake")
endif()
if(BUILD_TESTING)
  find_program(DRAWER_MAKE NAMES gmake make)
  if(DRAWER_MAKE)
    add_executable(check_placement tests/placement.cpp)
    target_compile_features(check_placement PRIVATE cxx_std_17)
    target_include_directories(check_placement SYSTEM PRIVATE ${OpenCASCADE_INCLUDE_DIR})
    target_link_libraries(check_placement PRIVATE TKernel TKMath TKBRep TKBO TKTopAlgo TKXSBase TKDESTEP TKXCAF TKLCAF TKCDF)
    # Keep the reference geometry in tests/placement.cpp independent of project defaults.
    add_test(NAME generate_positioned_triple COMMAND ${DRAWER_MAKE} -C "${CMAKE_CURRENT_SOURCE_DIR}" models SKIP_BUILD=1
      "DRAWER_BIN=$<TARGET_FILE:drawer>" "OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/pair"
      "BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}" "JSONNET=${JSONNET_EXECUTABLE}"
      FACE_WIDTH_MM=521 FACE_HEIGHT_MM=762 LOWER_POSITION_Y_MM=0 FACE_BOTTOM_OVERLAY_MM=16)
    set_tests_properties(generate_positioned_triple PROPERTIES FIXTURES_SETUP positioned_triple)
    add_test(NAME verify_positioned_triple COMMAND check_placement
      ${CMAKE_CURRENT_BINARY_DIR}/pair/inner-drawer-477x523x102.step
      ${CMAKE_CURRENT_BINARY_DIR}/pair/tall-front-bottom-aligned.step
      ${CMAKE_CURRENT_BINARY_DIR}/pair/cabinet-carcass.step)
    set_tests_properties(verify_positioned_triple PROPERTIES FIXTURES_REQUIRED positioned_triple)
  endif()
endif()
if(BUILD_TESTING)
  # Exercise configured geometry; no second construction implementation is kept
  # as a test oracle. The immutable STEP fixtures supply independent references.
  function(configured_test name assembly)
    add_test(NAME ${name} COMMAND drawer --project "${EXPANDED_PROJECT}"
      --assembly ${assembly} --output "${CMAKE_CURRENT_BINARY_DIR}/${name}.step" ${ARGN})
  endfunction()
  configured_test(default_drawer upper)
  configured_test(custom_drawer upper --set BOX_WIDTH_MM=500 --set BOX_DEPTH_MM=450 --set UPPER_HEIGHT_MM=130)
  configured_test(thin_bottom upper --set BOTTOM_THICKNESS_MM=6 --set DADO_DEPTH_MM=4 --set BOTTOM_OFFSET_MM=8)
  configured_test(deep_dado upper --set WALL_THICKNESS_MM=18 --set BOTTOM_THICKNESS_MM=9 --set DADO_DEPTH_MM=10 --set SEAT_DISTANCE_MM=16 --set SCREW_LENGTH_MM=25.4)
  configured_test(larger_hardware upper --set WALL_THICKNESS_MM=18 --set SCREW_LENGTH_MM=25.4 --set SEAT_DISTANCE_MM=16)
  configured_test(custom_face lower --set FACE_WIDTH_MM=510 --set FACE_HEIGHT_MM=750 --set FACE_THICKNESS_MM=18)
  configured_test(face_up lower --set FACE_TOP_Z_MM=751)
  configured_test(face_down lower --set FACE_TOP_Z_MM=691)
  configured_test(face_right lower --set FACE_OFFSET_X_MM=30)
  configured_test(custom_clearance upper --set POCKET_CLEARANCE_MM=3 --set SCREW_TIP_CLEARANCE_MM=1)
  configured_test(three_screws upper --set SCREWS_PER_CORNER=3)
  configured_test(one_screw upper --set SCREWS_PER_CORNER=1)
  configured_test(positioned_xyz upper --set UPPER_POSITION_X_MM=17 --set UPPER_POSITION_Y_MM=22 --set UPPER_POSITION_Z_MM=629)
  configured_test(positioned_negative carcass --set CARCASS_POSITION_X_MM=-20 --set FRAME_FRONT_Y_MM=-12 --set CARCASS_POSITION_Z_MM=-35)
  configured_test(carcass carcass)
  configured_test(carcass_custom carcass --set OPENING_WIDTH_MM=600 --set OPENING_HEIGHT_MM=800 --set CABINET_DEPTH_MM=650 --set CARCASS_THICKNESS_MM=18 --set STILE_WIDTH_MM=40 --set STILE_THICKNESS_MM=20 --set CARCASS_POSITION_X_MM=-10 --set FRAME_FRONT_Y_MM=5 --set CARCASS_POSITION_Z_MM=20)
  configured_test(carcass_flush_stiles carcass --set STILE_WIDTH_MM=18)

  function(rejected_geometry name assembly override)
    configured_test(${name} ${assembly} --set "${override}")
    set_tests_properties(${name} PROPERTIES WILL_FAIL TRUE)
  endfunction()
  rejected_geometry(reject_through_dado upper DADO_DEPTH_MM=12)
  rejected_geometry(reject_excess_dado upper DADO_DEPTH_MM=20)
  rejected_geometry(reject_screw_breakthrough upper SCREW_LENGTH_MM=40)
  rejected_geometry(reject_dado_collision upper SCREW_LOWER_MM=20)
  rejected_geometry(reject_rim_collision upper SCREW_UPPER_MM=102)
  rejected_geometry(reject_thin_wall_hardware upper WALL_THICKNESS_MM=10)
  rejected_geometry(reject_overlapping_pockets upper SCREW_UPPER_MM=40)
  rejected_geometry(reject_small_pocket upper POCKET_DIAMETER_MM=5)
  rejected_geometry(reject_fractional_count upper SCREWS_PER_CORNER=1.5)
  rejected_geometry(reject_zero_count upper SCREWS_PER_CORNER=0)
  rejected_geometry(reject_detached_face lower FACE_TOP_Z_MM=2000)
  rejected_geometry(reject_negative_clearance upper POCKET_CLEARANCE_MM=-1)
  rejected_geometry(reject_carcass_depth carcass CABINET_DEPTH_MM=18)
  rejected_geometry(reject_carcass_stile carcass STILE_WIDTH_MM=17)
  rejected_geometry(reject_fractional_board_start carcass CARCASS_BOARD_START=12.5)
  rejected_geometry(reject_zero_board_start carcass CARCASS_BOARD_START=0)

  add_test(NAME project_cli COMMAND ${CMAKE_COMMAND}
    "-DDRAWER=$<TARGET_FILE:drawer>" "-DPROJECT=${EXPANDED_PROJECT}"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/cli-tests" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.cmake")
  add_test(NAME help_manual COMMAND drawer --help)
  set_tests_properties(help_manual PROPERTIES PASS_REGULAR_EXPRESSION "EXIT STATUS")
endif()

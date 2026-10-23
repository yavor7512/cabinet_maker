file(MAKE_DIRECTORY "${OUTPUT}")
function(run)
  execute_process(COMMAND ${ARGV} RESULT_VARIABLE status OUTPUT_VARIABLE log ERROR_VARIABLE errors)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "Project equivalence failed (${status}): ${errors}\n${log}")
  endif()
endfunction()
# Independent legacy CLI reference: these values intentionally do not read the
# project configuration. Comparisons include occupied material, not just bounds.
set(common --width 453 --depth 499 --thickness 12 --bottom-thickness 12 --dado-depth 6 --bottom-offset 6
  --screws-per-corner 2 --screw-length 19.05 --screw-diameter 3.5 --head-diameter 5.5 --head-height 1.8
  --tip-length 3 --pocket-diameter 7 --pocket-angle 15 --seat-distance 10.5 --screw-lower 35 --screw-upper 65
  --pocket-clearance 2 --screw-tip-clearance 0.5)
run("${DRAWER}" ${common} --height 84 --no-face --position-y 12 --position-z 619 --board-start 1
  --assembly-name "Upper inner drawer" --output "${OUTPUT}/upper-reference.step")
run("${DRAWER}" ${common} --height 185 --face --face-width 469.9 --face-height 711.2 --face-thickness 12
  --face-offset 228.9 --position-y 12 --position-z 35 --board-start 6
  --assembly-name "Lower pull-out tray with tall front" --output "${OUTPUT}/lower-reference.step")
run("${DRAWER}" --model carcass --opening-width 494 --opening-height 737 --cabinet-depth 597
  --carcass-thickness 18 --stile-width 38 --stile-thickness 18 --position-x -8.5 --position-y 3.5 --board-start 12
  --assembly-name "Cabinet carcass and face frame" --output "${OUTPUT}/carcass-reference.step")
foreach(assembly upper lower carcass)
  run("${DRAWER}" --project "${PROJECT}" --assembly "${assembly}" --output "${OUTPUT}/${assembly}-project.step")
  run("${COMPARE}" --compare "${OUTPUT}/${assembly}-reference.step" "${OUTPUT}/${assembly}-project.step")
endforeach()
message(STATUS "All 33 named parts match the legacy model in bounds, volume and occupied material")
run("${COMPARE}" --compare "${FIXTURES}/inner-drawer-477x523x102.step" "${OUTPUT}/upper-project.step")
run("${COMPARE}" --compare "${FIXTURES}/tall-front-bottom-aligned.step" "${OUTPUT}/lower-project.step")
run("${COMPARE}" --compare "${FIXTURES}/cabinet-carcass.step" "${OUTPUT}/carcass-project.step")
message(STATUS "All 33 named parts also match the frozen pre-configuration STEP files")

# Runs ravel-run twice on the same scenario into two directories and requires the two
# output trees to be byte-identical, including manifest.json.
#
# Usage:
#   cmake -DRUN=<ravel-run> -DSCENARIO=<file> -DOUT=<dir> -DWORKDIR=<repo root>
#         [-DSECOND_COMMIT=<sha>] -P run_twice.cmake
# SECOND_COMMIT exists only to show that the check can fail (a different commit changes the manifest).

foreach(var IN ITEMS RUN SCENARIO OUT WORKDIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "run_twice.cmake: ${var} is not set")
  endif()
endforeach()
if(NOT DEFINED SECOND_COMMIT)
  set(SECOND_COMMIT same)
endif()

file(REMOVE_RECURSE "${OUT}")
foreach(pass IN ITEMS first second)
  if(pass STREQUAL "first")
    set(commit same)
  else()
    set(commit "${SECOND_COMMIT}")
  endif()
  execute_process(
    COMMAND "${RUN}" "${SCENARIO}" "${OUT}/${pass}" "${commit}"
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE run_result
    OUTPUT_QUIET)
  if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "ravel-run (${pass} pass) exited with: ${run_result}")
  endif()
endforeach()

file(GLOB_RECURSE first_files RELATIVE "${OUT}/first" "${OUT}/first/*")
file(GLOB_RECURSE second_files RELATIVE "${OUT}/second" "${OUT}/second/*")
list(SORT first_files)
list(SORT second_files)
if(NOT first_files STREQUAL second_files)
  message(FATAL_ERROR "the two runs wrote different file sets")
endif()
list(LENGTH first_files count)
if(count EQUAL 0)
  message(FATAL_ERROR "no output files under ${OUT}/first")
endif()
foreach(f IN LISTS first_files)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUT}/first/${f}" "${OUT}/second/${f}"
    RESULT_VARIABLE compare_result)
  if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "${f} differs between runs\nRun: diff -u \"${OUT}/first/${f}\" \"${OUT}/second/${f}\"")
  endif()
endforeach()
message(STATUS "${count} files byte-identical across two runs")

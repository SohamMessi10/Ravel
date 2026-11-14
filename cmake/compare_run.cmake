# Runs ravel-run for one scenario and byte-compares every file under GOLDEN with the
# same relative path under OUT/NAME. manifest.json is not in the golden set, because it
# records the build (compiler, flags, commit), which differs between presets.
#
# Usage:
#   cmake -DRUN=<ravel-run> -DSCENARIO=<file> -DNAME=<scenario name> -DOUT=<dir>
#         -DGOLDEN=<dir> -DWORKDIR=<repo root> -P compare_run.cmake

foreach(var IN ITEMS RUN SCENARIO NAME OUT GOLDEN WORKDIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "compare_run.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${OUT}")
execute_process(
  COMMAND "${RUN}" "${SCENARIO}" "${OUT}" golden
  WORKING_DIRECTORY "${WORKDIR}"
  RESULT_VARIABLE run_result
  OUTPUT_QUIET)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "ravel-run exited with: ${run_result}")
endif()

file(GLOB_RECURSE golden_files RELATIVE "${GOLDEN}" "${GOLDEN}/*")
list(LENGTH golden_files count)
if(count EQUAL 0)
  message(FATAL_ERROR "no golden files under ${GOLDEN}")
endif()
foreach(f IN LISTS golden_files)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${GOLDEN}/${f}" "${OUT}/${NAME}/${f}"
    RESULT_VARIABLE compare_result)
  if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "${f} differs from golden\nRun: diff -u \"${GOLDEN}/${f}\" \"${OUT}/${NAME}/${f}\"")
  endif()
endforeach()

# Runs PROGRAM with INPUT as its only argument, writes stdout to ACTUAL,
# and fails unless ACTUAL is byte-identical to EXPECTED.
#
# Usage:
#   cmake -DPROGRAM=<exe> -DINPUT=<file> -DEXPECTED=<file> -DACTUAL=<file> -P compare_output.cmake

foreach(var IN ITEMS PROGRAM INPUT EXPECTED ACTUAL)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "compare_output.cmake: ${var} is not set")
  endif()
endforeach()

execute_process(
  COMMAND "${PROGRAM}" "${INPUT}"
  OUTPUT_FILE "${ACTUAL}"
  RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "${PROGRAM} exited with: ${run_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files "${EXPECTED}" "${ACTUAL}"
  RESULT_VARIABLE compare_result)
if(NOT compare_result EQUAL 0)
  message(FATAL_ERROR "Output differs from golden file\n  expected: ${EXPECTED}\n  actual:   ${ACTUAL}\nRun: diff -u \"${EXPECTED}\" \"${ACTUAL}\"")
endif()

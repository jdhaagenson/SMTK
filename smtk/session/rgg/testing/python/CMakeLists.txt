

# tests that require SMTK_DATA_DIR
set(smtkRGGPythonDataTests
  readRXFFileOp)

if (ENABLE_PYARC_BINDINGS)
  list(APPEND smtkRGGPythonDataTests
    exportToPyARCOp)
endif()

#only run these tests if we have a valid data directory
if (SMTK_DATA_DIR)
  foreach (test ${smtkRGGPythonDataTests})
    smtk_add_test_python(${test}Py ${test}.py
      --data-dir=${SMTK_DATA_DIR} )
    set_tests_properties( ${test}Py PROPERTIES LABELS "RGGSession" )
  endforeach()
endif()

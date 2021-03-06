set(remusExtSource
  MeshOperation.cxx
  MeshServerLauncher.cxx)

set(remusExtHeaders
  MeshOperation.h
  MeshServerLauncher.h)

smtk_operation_xml("${CMAKE_CURRENT_SOURCE_DIR}/MeshOperation.sbt" remusOperationXML)

add_library(smtkRemusExt ${remusExtSource})
target_link_libraries(smtkRemusExt
  LINK_PRIVATE
    RemusClient
    RemusCommon
    RemusServer
    smtkCore
    ${Boost_LIBRARIES}
)

target_include_directories(smtkRemusExt PRIVATE ${REMUS_INCLUDE_DIRS})
smtk_export_header(smtkRemusExt smtkRemusExtExports.h)
smtk_public_headers(smtkRemusExt ${remusExtHeaders})
smtk_install_library(smtkRemusExt)

if (SMTK_ENABLE_PYTHON_WRAPPING)
  add_library(smtkRemusExtPy MODULE smtkRemusExtPy.cxx)
  target_include_directories(smtkRemusExtPy PRIVATE ${PYTHON_INCLUDE_DIRS})
  target_link_libraries(smtkRemusExtPy
    smtkRemusExt
    ${PYTHON_LIBRARIES}
  )
  set_target_properties(smtkRemusExtPy PROPERTIES PREFIX "")

  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(ignore_rules_for_python "")
    # disables: warning C4273: inconsistent dll linkage
    set(ignore_rules_for_python "${ignore_rules_for_python} /wd4273")
    set_property(TARGET smtkRemusExtPy APPEND_STRING PROPERTY COMPILE_FLAGS
                 ${ignore_rules_for_python})
  endif()
endif()


add_subdirectory(plugin)

if (SMTK_ENABLE_PYTHON_WRAPPING)
  add_subdirectory(pybind11)
endif()

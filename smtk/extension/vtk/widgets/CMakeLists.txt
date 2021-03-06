set(VTKLibSrcs
  vtkSBFunctionParser.cxx
  vtkSMTKArcRepresentation.cxx
)

set(VTKLibHeaders
  vtkSBFunctionParser.h
  vtkSMTKArcRepresentation.h
)
set (__dependencies)

# Test for targets that might be required or
# might not exist.
foreach(target
    vtkInteractionStyle
    vtkRenderingFreeType
    vtkRenderingOpenGL2
    vtkRenderingVolumeOpenGL2
    vtkRenderingGL2PSOpenGL2
)
  if (TARGET ${target})
    list(APPEND __dependencies ${target})
  endif()
endforeach()

# Add compile definitions to force linking to implementation modules
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS ${VTK_DEFINITIONS})

# Don't do VTK header test. It is broken at the moment.
set(${vtk-module}_NO_HeaderTest 1)
set(VTK_INSTALL_NO_HEADERS 1)
vtk_module_library(${vtk-module} ${VTKLibSrcs})
target_include_directories(${vtk-module}
  PUBLIC
    $<BUILD_INTERFACE:${smtk_SOURCE_DIR}>
    $<BUILD_INTERFACE:${smtk_BINARY_DIR}>
    $<INSTALL_INTERFACE:include/smtk/${SMTK_VERSION}>)

target_link_libraries(${vtk-module}
  LINK_PUBLIC
  ${__dependencies}
  )

#has to be after the vtk_module_library
smtk_export_header(${vtk-module} Exports.h)

target_include_directories(${vtk-module}
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

#install the headers
smtk_public_headers(${vtk-module} ${VTKLibHeaders})
# ... and make header compilation tests link properly:
smtk_install_library(${vtk-module})

if (SMTK_ENABLE_PARAVIEW_SUPPORT)
  vtk_add_cs_wrapping(${vtk-module})
  smtk_install_library(${vtk-module}CS)
endif ()

# Potentially wrap in Python using VTK's wrapping methods.
# This makes the filters available for use with other VTK-wrapped filters.
#if (SMTK_ENABLE_PYTHON_WRAPPING AND VTK_WRAP_PYTHON)
#  vtk_smtk_wrap_module_python3(${vtk-module} "${VTKLibHeaders}")
#endif()

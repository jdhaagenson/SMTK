set (__dependencies)

# Test for targets that might be required or
# might not exist.
foreach(target
    vtkInteractionStyle
    vtkRenderingFreeType
    vtkRenderingOpenGL2
    vtkRenderingGL2PSOpenGL2
)
  if (TARGET ${target})
    list(APPEND __dependencies ${target})
  endif()
endforeach()

vtk_module(vtkSMTKSourceExt
  DEPENDS
    vtkCommonCore
    vtkCommonDataModel
    vtkCommonExecutionModel
    vtkRenderingCore
    vtkInteractionStyle
    vtkViewsCore
    vtkViewsInfovis
    vtkIOXML
    vtkIOLegacy
  PRIVATE_DEPENDS
    ${__dependencies}
  TEST_DEPENDS
    vtkTestingRendering
  EXCLUDE_FROM_WRAP_HIERARCHY
)
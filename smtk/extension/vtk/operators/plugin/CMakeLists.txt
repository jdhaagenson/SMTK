if (SMTK_ENABLE_PARAVIEW_SUPPORT)

  include(${PARAVIEW_USE_FILE})
  include (ParaViewPlugins)

  # We need to add the current value of VTK_MODULES_DIR to the module path
  # so that when the plugins are built all the modules can be found. Otherwise,
  # modules that aren't loaded as direct dependencies of CMB modules will
  # not be found.
  list(APPEND CMAKE_MODULE_PATH "${VTK_MODULES_DIR}")

  add_paraview_plugin(
    smtkVTKOperationsPlugin "1.0"
    SERVER_SOURCES
    sessionInit.cxx
    )

  target_link_libraries(smtkVTKOperationsPlugin
    LINK_PUBLIC
      smtkCore
      vtkPVServerManagerApplication
    LINK_PRIVATE
      vtkSMTKOperationsExt
      vtkPVServerManagerApplicationCS
    )
  smtk_install_library(smtkVTKOperationsPlugin)

endif ()

if (NOT DEFINED conda_prefix)
  set(conda_prefix $ENV{CONDA_PREFIX})
endif()

set(CMAKE_SYSTEM_NAME "${CMAKE_HOST_SYSTEM_NAME}")
set(CMAKE_SYSROOT "${conda_prefix}/$ENV{HOST}/sysroot")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
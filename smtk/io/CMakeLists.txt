# set up sources to build
set(ioSrcs
  AttributeReader.cxx
  AttributeWriter.cxx
  Helpers.cxx
  json/jsonComponentSet.cxx
  json/jsonSelectionMap.cxx
  Logger.cxx
  ModelToMesh.cxx
  XmlDocV1Parser.cxx
  XmlDocV2Parser.cxx
  XmlDocV3Parser.cxx
  XmlV2StringWriter.cxx
  XmlV3StringWriter.cxx
)

set(ioHeaders
  AttributeReader.h
  AttributeWriter.h
  Helpers.h
  json/jsonComponentSet.h
  json/jsonSelectionMap.h
  Logger.h
  ModelToMesh.h
  #XmlDocV1Parser.h
  XmlDocV2Parser.h
  XmlStringWriter.h
  XmlV2StringWriter.h
  XmlV3StringWriter.h
)

# mesh-related I/O
list(APPEND ioSrcs
  ExportMesh.cxx
  ImportMesh.cxx
  ReadMesh.cxx
  WriteMesh.cxx
  mesh/Format.cxx
  mesh/MeshIOMoab.cxx
  mesh/MeshIOXMS.cxx
)

list(APPEND ioHeaders
  ExportMesh.h
  ImportMesh.h
  ReadMesh.h
  WriteMesh.h
  mesh/Format.h
  mesh/MeshIO.h
  mesh/MeshIOMoab.h
  mesh/MeshIOXMS.h
)

#install the headers
smtk_public_headers(smtkCore ${ioHeaders})

if (SMTK_ENABLE_PYTHON_WRAPPING)
  add_subdirectory(pybind11)
endif()

if (SMTK_ENABLE_TESTING)
  add_subdirectory(testing)
endif()

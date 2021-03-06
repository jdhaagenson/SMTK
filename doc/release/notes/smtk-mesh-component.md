# Introduce smtk::mesh::Component

To adhere to the Resource/Component paradigm in SMTK 3.0, we have
introduced the lightweight class smtk::mesh::Component, which
facilitates the ability to pass meshsets as component items in an
attribute.

### User-facing changes

`smtk::mesh::Component` is now used to send/retrieve meshsets to/from
attributes.

Mesh resources and components now appear in the resource tree view
and mesh component names may be edited in place using the new
SetMeshName operator.

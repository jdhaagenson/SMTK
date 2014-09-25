"""
Manual port of SMTK/smtk/attribute/Testing/basicAttributeTest.cxx
For verifying python-shiboken wrappers

Requires SMTKCorePython.so to be in module path
"""

import smtk

if __name__ == '__main__':
    import sys

    status = 0

    manager = smtk.attribute.Manager()
    print 'Manager created'
    t = manager.resourceType()
    if t != smtk.util.Resource.ATTRIBUTE:
      print 'ERROR: Returned wrong resource type'
      status = -1
    print 'Resource type:', smtk.util.Resource.type2String(t)
    def_ = manager.createDefinition('testDef')
    if def_ is not None:
        print 'Definition testDef created'
    else:
        print 'ERROR: Definition testDef not created'
        status = -1
    def1 = manager.createDefinition("testDef")
    if def1 is None:
        print 'Duplicated definition testDef not created'
    else:
        print 'ERROR: Duplicated definition testDef created'
        status = -1
    att = manager.createAttribute('testAtt', 'testDef')
    if not att is None:
        print 'Attribute testAtt created'
    else:
        print 'ERROR: Attribute testAtt not created'
        status = -1

    att1 = manager.createAttribute('testAtt', 'testDef')
    if att1 is None:
        print 'Duplicate Attribute testAtt not created'
    else:
        print 'ERROR: Duplicate Attribute testAtt  created'
        status = -1

    if att.id() != 0:
      print "Unexpected id should be 0"
      status = -1
    if att.isColorSet():
      print "Color should not be set."
      status = -1
    att.setColor(3, 24, 12, 6)
    tcol = att.color()
    if not att.isColorSet():
       print "Color should be set.\n"
       status = -1
    att.unsetColor()
    if att.isColorSet():
       print "Color should not be set.\n"
       status = -1
    if att.numberOfAssociatedEntities() != 0:
       print "Should not have associated entities.\n"
       status = -1
    if len(att.associatedEntitiesSet()) != 0 :
       print "Should not have associated entities.\n"
       status = -1
    if att.appliesToBoundaryNodes():
       print "Should not be applies to boundry node.\n"
       status = -1
    att.setAppliesToBoundaryNodes(True)
    if not att.appliesToBoundaryNodes():
       print "Should be applies to boundry node.\n"
       status = -1
    att.setAppliesToBoundaryNodes(False)
    if att.appliesToBoundaryNodes():
       print "Should not be applies to boundry node.\n"
       status = -1
    if att.appliesToInteriorNodes():
       print "Should not be applies to interior node.\n"
       status = -1
    att.setAppliesToInteriorNodes(True)
    if not att.appliesToInteriorNodes():
       print "Should be applies to interior node.\n"
       status = -1
    att.setAppliesToInteriorNodes(False)
    if att.appliesToInteriorNodes():
       print "Should not applies to interior node.\n"
       status = -1
#    if att.manager() is not None:
#       print "Should not be null.\n"
#       status = -1
    del manager
    print 'Manager destroyed'

    sys.exit(status)
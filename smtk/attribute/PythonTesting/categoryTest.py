"""
Manual port of SMTK/smtk/attribute/Testing/categoryTest.cxx
For verifying python-shiboken wrappers

Requires SMTKCorePython.so to be in module path
"""

import smtk

def addItemDefinition( ato, data_type, name):
  def_ = data_type.New(name)
  if def_ is None:
    print "could not create"
    return None
  idef = data_type.ToItemDefinition(def_)
  if idef is None:
    print "could not convert"
    return None
  if not ato.addItemDefinition(idef):
    print "could not add"
    return None
  return def_


if __name__ == '__main__':
    import sys

    status = 0
    
    manager = smtk.attribute.Manager()
    print 'Manager created'
    
    expDef = manager.createDefinition("ExpDef");
    expDef.setBriefDescription("Sample Expression");
    expDef.setDetailedDescription("Sample Expression for testing\nThere is not much here!");
    eitemdef = addItemDefinition(expDef, smtk.attribute.StringItemDefinition, "Expression String")
    eitemdef2 = addItemDefinition( expDef, smtk.attribute.StringItemDefinition, "Aux String")
    eitemdef.setDefaultValue("sample")
    
    base = manager.createDefinition("BaseDef")
    #Lets add some item definitions
    iitemdef = addItemDefinition( base, smtk.attribute.IntItemDefinition, "IntItem1");
    iitemdef.addCategory("Flow")
    iitemdef = addItemDefinition( base, smtk.attribute.IntItemDefinition, "IntItem2");
    iitemdef.setDefaultValue(10);
    iitemdef.addCategory("Heat");
    
    def1 = manager.createDefinition("Derived1", "BaseDef");
    # Lets add some item definitions
    ditemdef = addItemDefinition( def1, smtk.attribute.DoubleItemDefinition, "DoubleItem1")
    # Allow this one to hold an expression
    ditemdef.addCategory("Veg");
    ditemdef.setExpressionDefinition(expDef);
    # Check to make sure we can use expressions
    if not ditemdef.allowsExpressions():
      print "ERROR - Item Def does not allow expressions"
      status = -1;
    ditemdef = addItemDefinition( def1, smtk.attribute.DoubleItemDefinition, "DoubleItem2")
    ditemdef.setDefaultValue(-35.2);
    ditemdef.addCategory("Constituent");
    
    def2 = manager.createDefinition("Derived2", "Derived1");
    # Lets add some item definitions
    sitemdef = addItemDefinition( def2, smtk.attribute.StringItemDefinition, "StringItem1");
    sitemdef.addCategory("Flow");
    sitemdef = addItemDefinition( def2, smtk.attribute.StringItemDefinition, "StringItem2");
    sitemdef.setDefaultValue("Default");
    sitemdef.addCategory("General");
    
    # Process Categories
    manager.updateCategories();
    # Lets see what categories the attribute definitions think they are
    if expDef.numberOfCategories() != 0:
      categories = expDef.categories();
      print "ERROR: ExpDef's categories: "
      for it in categories:
        print "\t \"%s\""%it
    else:
      print "ExpDef has no categories"
    if def2.numberOfCategories() != 0:
      categories = def2.categories();
      print "Def2's categories: ";
      for it in categories:
        print "\t \"%s\""%it
    else:
      print "ERROR: Def2 has no categories!";
    # Lets test creating an attribute by passing in the expression definition explicitly
    expAtt = manager.createAttribute("Exp1", expDef);
    att = manager.createAttribute("testAtt", "Derived2");
    if att is not None:
      print "Attribute testAtt created"
    else:
      print "ERROR: Attribute testAtt not created";
      status = -1;
      
    # Find the expression enabled item
    item = att.item(2);
    vitem = smtk.attribute.ValueItem.CastTo(item);
    if vitem.allowsExpressions():
      vitem.setExpression(expAtt);
      print "Expression Set on %s"%vitem.name();
    else:
      print "ERROR: Can not set expression on %s"%vitem.name()
      status = -1;
  
    n = att.numberOfItems();
    print "Items of testAtt:\n";
    for i in range(0,n,1):
      item = att.item(i);
      print "\t %s Type = %s"%(item.name(), smtk.attribute.Item.type2String(item.type()));
      vitem = smtk.attribute.ValueItem.CastTo(item);
      if vitem is not None:
        if vitem.isExpression(0):
          print "\t\tusing Expression: %s"%vitem.expression(0).name();
        else:
          print "\t\tValue = %s"%vitem.valueAsString();
    writer = smtk.attribute.XmlV1StringWriter(manager);
    print writer.convertToString();

    del manager
    print 'Manager destroyed'

    sys.exit(status)
//  MeshGeom.cc - A group of Tets in 3 space
//  Written by:
//   Eric Kuehne
//   Department of Computer Science
//   University of Utah
//   April 2000
//  Copyright (C) 2000 SCI Institute

#include <Core/Datatypes/MeshGeom.h>

namespace SCIRun {

PersistentTypeID MeshGeom::type_id("MeshGeom", "Datatype", 0);

DebugStream MeshGeom::dbg("MeshGeom", true);

MeshGeom::MeshGeom()
{
}

string
MeshGeom::getInfo()
{
  ostringstream retval;
  retval << "name = " << d_name << endl;
  return retval.str();
}

void
MeshGeom::io(Piostream&)
{
}


} // End namespace SCIRun

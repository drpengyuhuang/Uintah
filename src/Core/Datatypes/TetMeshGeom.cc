//  TetMeshGeom.cc - A group of Tets in 3 space
//  Written by:
//   Eric Kuehne
//   Department of Computer Science
//   University of Utah
//   April 2000
//  Copyright (C) 2000 SCI Institute

#include <Core/Datatypes/TetMeshGeom.h>

namespace SCIRun {

//////////
// PIO support
static Persistent* maker(){
  return new TetMeshGeom();
}

string TetMeshGeom::typeName(int){
  static string className = "TetMeshGeom";
  return className;
}

PersistentTypeID TetMeshGeom::type_id(TetMeshGeom::typeName(0), 
				      MeshGeom::typeName(0), 
				      maker);

DebugStream TetMeshGeom::dbg("TetMeshGeom", true);

#define TETMESHGEOM_VERSION 1
void TetMeshGeom::io(Piostream& stream){
  stream.begin_class(typeName(0).c_str(), TETMESHGEOM_VERSION);
  MeshGeom::io(stream);
#if 0
  // Commented out to fix compilation problem on linux - Steve
  Pio(stream, tets_);
#endif
  stream.end_class();
}

//////////
// Constructors/Destructor
TetMeshGeom::TetMeshGeom(){
}

TetMeshGeom::TetMeshGeom(const vector<NodeSimp>& inodes, const vector<TetSimp>& itets):
  has_neighbors(0)
{
  node_ = inodes;
  tets_ = itets;
}

TetMeshGeom::~TetMeshGeom(){
}

string
TetMeshGeom::get_info(){
  return string();
}


void TetMeshGeom::set_tets(const vector<TetSimp>& itets){
  tets_.clear();
  tets_ = itets;
}

string TetMeshGeom::getTypeName(int n){
  return typeName(n);
}


} // End namespace SCIRun

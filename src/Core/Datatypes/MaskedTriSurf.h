/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.
*/

/*
 *  MaskedTriSurf.h
 *
 *  Written by:
 *   Martin Cole
 *   School of Computing
 *   University of Utah
 *
 *  Copyright (C) 2001 SCI Institute
 */

#ifndef Datatypes_MaskedTriSurf_h
#define Datatypes_MaskedTriSurf_h

#include <Core/Datatypes/Field.h>
#include <Core/Datatypes/TriSurf.h>
#include <Core/Datatypes/GenericField.h>
#include <Core/Containers/LockingHandle.h>
#include <Core/Persistent/PersistentSTL.h>
#include <vector>

namespace SCIRun {

template <class T> 
class MaskedTriSurf : public TriSurf<T> {
private:
  vector<char> mask_;  // since Pio isn't implemented for bool's
public:
  vector<char>& mask() { return mask_; }

  MaskedTriSurf() :
    TriSurf<T>() {};
  MaskedTriSurf(Field::data_location data_at) : 
    TriSurf<T>(data_at) {};
  MaskedTriSurf(TriSurfMeshHandle mesh, Field::data_location data_at) : 
    TriSurf<T>(mesh, data_at) 
  {
    resize_fdata();
  };

  bool get_valid_nodes_and_data(vector<TriSurfMesh::Node::index_type> &nodes,
				vector<T> &data) {
    nodes.resize(0);
    data.resize(0);
    if (data_at() != NODE) return false;
    TriSurfMesh::Node::iterator ni, nie;
    get_typed_mesh()->begin(ni);
    get_typed_mesh()->end(nie);
    for (; ni != nie; ++ni) { 
      if (mask_[*ni]) { nodes.push_back(*ni); data.push_back(fdata()[*ni]); }
    }
    return true;
  }

  virtual ~MaskedTriSurf() {};

  bool value(T &val, typename TriSurfMesh::Node::index_type i) const
  { if (!mask_[i]) return false; val = fdata()[i]; return true; }
  bool value(T &val, typename TriSurfMesh::Edge::index_type i) const
  { if (!mask_[i]) return false; val = fdata()[i]; return true; }
  bool value(T &val, typename TriSurfMesh::Cell::index_type i) const
  { if (!mask_[i]) return false; val = fdata()[i]; return true; }

  void    io(Piostream &stream);

  void initialize_mask(char masked) {
    for (vector<char>::iterator c = mask_.begin(); c != mask_.end(); ++c) *c=masked;
  }

  void resize_fdata() {
    if (data_at() == NODE)
    {
      typename mesh_type::Node::size_type ssize;
      get_typed_mesh()->size(ssize);
      mask_.resize(ssize);
    }
    else if (data_at() == EDGE)
    {
      typename mesh_type::Edge::size_type ssize;
      get_typed_mesh()->size(ssize);
      mask_.resize(ssize);
    }
    else if (data_at() == FACE)
    {
      typename mesh_type::Face::size_type ssize;
      get_typed_mesh()->size(ssize);
      mask_.resize(ssize);
    }
    else if (data_at() == CELL)
    {
      typename mesh_type::Cell::size_type ssize;
      get_typed_mesh()->size(ssize);
      mask_.resize(ssize);
    }
    else
    {
      ASSERTFAIL("data at unrecognized location");
    }
    TriSurf<T>::resize_fdata();
  }

  static  PersistentTypeID type_id;
  static const string type_name(int n = -1);
  virtual const string get_type_name(int n = -1) const { return type_name(n); }
  virtual const TypeDescription* get_type_description() const;
private:
  static Persistent *maker();
};

// Pio defs.
const int MASKED_TRI_SURF_VERSION = 1;

template <class T>
Persistent*
MaskedTriSurf<T>::maker()
{
  return scinew MaskedTriSurf<T>;
}

template <class T>
PersistentTypeID 
MaskedTriSurf<T>::type_id(type_name(-1), 
			 TriSurf<T>::type_name(-1),
			 maker);


template <class T>
void 
MaskedTriSurf<T>::io(Piostream& stream)
{
  stream.begin_class(type_name(-1), MASKED_TRI_SURF_VERSION);
  TriSurf<T>::io(stream);
  Pio(stream, mask_);
  stream.end_class();
}

template <class T> 
const string 
MaskedTriSurf<T>::type_name(int n)
{
  ASSERT((n >= -1) && n <= 1);
  if (n == -1)
  {
    static const string name = type_name(0) + FTNS + type_name(1) + FTNE;
    return name;

  }
  else if (n == 0)
  {
    return "MaskedTriSurf";
  }
  else
  {
    return find_type_name((T *)0);
  }
}


template <class T>
const TypeDescription* 
get_type_description(MaskedTriSurf<T>*)
{
  static TypeDescription* td = 0;
  if(!td){
    const TypeDescription *sub = SCIRun::get_type_description((T*)0);
    TypeDescription::td_vec *subs = scinew TypeDescription::td_vec(1);
    (*subs)[0] = sub;
    td = scinew TypeDescription("MaskedTriSurf", subs, __FILE__, "SCIRun");
  }
  return td;
}

template <class T>
const TypeDescription* 
MaskedTriSurf<T>::get_type_description() const 
{
  return SCIRun::get_type_description((MaskedTriSurf<T>*)0);
}

} // end namespace SCIRun

#endif // Datatypes_MaskedTriSurf_h


/*
 *  GeomTexSlices.cc
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   June 1995
 *
 *  Copyright (C) 1995 SCI Group
 */

#include <Core/Geom/GeomTexSlices.h>
#include <Core/Containers/String.h>
#include <Core/Geometry/BBox.h>
#include <Core/Malloc/Allocator.h>
#ifdef _WIN32
#include <string.h>
#else
#include <strings.h>
#endif
#include <iostream>
using std::ostream;

namespace SCIRun {

Persistent* make_GeomTexSlices()
{
    return scinew GeomTexSlices(0,0,0,Point(0,0,0), Point(1,1,1));
}

PersistentTypeID GeomTexSlices::type_id("GeomTexSlices", "GeomObj", make_GeomTexSlices);

GeomTexSlices::GeomTexSlices(int nx, int ny, int nz, const Point& min,
			     const Point &max)
: nx(nx), ny(ny), nz(nz), min(min), max(max), have_drawn(0), accum(0.1),
  bright(0.6)
{
    Xmajor.newsize(nx, ny, nz);
    Ymajor.newsize(ny, nx, nz);
    Zmajor.newsize(nz, nx, ny);
}

GeomTexSlices::GeomTexSlices(const GeomTexSlices& copy)
: GeomObj(copy)
{
}


GeomTexSlices::~GeomTexSlices()
{

}

void GeomTexSlices::get_bounds(BBox& bb)
{
  bb.extend(min);
  bb.extend(max);
}

GeomObj* GeomTexSlices::clone()
{
    return scinew GeomTexSlices(*this);
}

#define GeomTexSlices_VERSION 1

void GeomTexSlices::io(Piostream& stream)
{
    stream.begin_class("GeomTexSlices", GeomTexSlices_VERSION);
    GeomObj::io(stream);
    stream.end_class();
}    

bool GeomTexSlices::saveobj(ostream&, const clString& /*format*/, GeomSave*)
{
  return 0;
}

} // End namespace SCIRun


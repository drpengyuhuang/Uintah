
/*
 *  TimeGroup.cc:  TimeGroups of GeomObj's
 *
 *  Written by:
 *   Steven G. Parker & David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   April 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Core/Geom/GeomTimeGroup.h>
#include <Core/Containers/Array2.h>
#include <Core/Containers/String.h>
#include <Core/Malloc/Allocator.h>
#include <iostream>
using std::cerr;
using std::ostream;
#ifdef _WIN32
#include <float.h>
#define MAXDOUBLE DBL_MAX
#else
#include <values.h>
#endif

namespace SCIRun {

static Persistent* make_GeomTimeGroup()
{
    return scinew GeomTimeGroup;
}

PersistentTypeID GeomTimeGroup::type_id("GeomTimeGroup", "GeomObj", make_GeomTimeGroup);

GeomTimeGroup::GeomTimeGroup(int del_children)
: GeomObj(), objs(0, 100), start_times(0,100),del_children(del_children)
{
}

GeomTimeGroup::GeomTimeGroup(const GeomTimeGroup& copy)
: GeomObj(copy), del_children(copy.del_children)
{
    objs.grow(copy.objs.size());
    start_times.grow(copy.start_times.size());
    for(int i=0;i<objs.size();i++){
	GeomObj* cobj=copy.objs[i];
	objs[i]=cobj->clone();
	start_times[i] = copy.start_times[i];
    }
}

void GeomTimeGroup::add(GeomObj* obj,double time)
{
    objs.add(obj);
    start_times.add(time);
}

void GeomTimeGroup::remove(GeomObj* obj)
{
   for(int i=0;i<objs.size();i++)
      if (objs[i] == obj) {
	 objs.remove(i);
	 start_times.remove(i);
	 if(del_children)delete obj;
	 break;
      }
}

void GeomTimeGroup::remove_all()
{
   if(del_children)
      for(int i=0;i<objs.size();i++)
	 delete objs[i];
   objs.remove_all();
   start_times.remove_all();
}

int GeomTimeGroup::size()
{
    return objs.size();
}

GeomObj* GeomTimeGroup::clone()
{
    return scinew GeomTimeGroup(*this);
}

void GeomTimeGroup::get_bounds(BBox& in_bb)
{

    for(int i=0;i<objs.size();i++)
	objs[i]->get_bounds(in_bb);

#if 0
    in_bb.extend(bbox);
#endif
}

GeomTimeGroup::~GeomTimeGroup()
{
    if(del_children){
	for(int i=0;i<objs.size();i++)
	    delete objs[i];
    }
}

void GeomTimeGroup::setbbox(BBox& b)
{
  bbox = b;
}

void GeomTimeGroup::reset_bbox()
{
    for(int i=0;i<objs.size();i++)
	objs[i]->reset_bbox();
}

#define GEOMTimeGroup_VERSION 1

void GeomTimeGroup::io(Piostream& stream)
{

    stream.begin_class("GeomTimeGroup", GEOMTimeGroup_VERSION);
    // Do the base class first...
    GeomObj::io(stream);
    Pio(stream, del_children);
    Pio(stream, objs);
    Pio(stream,start_times);
    stream.end_class();
}

bool GeomTimeGroup::saveobj(ostream& out, const clString& format,
			    GeomSave* saveinfo)
{
    static int cnt = 0;
    cnt++;
    cerr << "saveobj TimeGroup " << cnt << "\n";

    for(int i=0;i<objs.size();i++){ cerr << cnt << ">";
	if(!objs[i]->saveobj(out, format, saveinfo))
	  { cnt--;
	    return false;
	  }
    }
    cnt--;
    return true;
}

} // End namespace SCIRun


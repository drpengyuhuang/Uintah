
/*
 *  LabelSurface.cc:  Label a specific surf in a surftree
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   July 1997
 *
 *  Copyright (C) 1997 SCI Group
 *
 */

#include <config.h>
#include <Classlib/String.h>
#include <Classlib/NotFinished.h>
#include <Dataflow/Module.h>
#include <Datatypes/BasicSurfaces.h>
#include <Datatypes/SurfacePort.h>
#include <Datatypes/SurfTree.h>
#include <TCL/TCLvar.h>
#include <iostream.h>
#include <Malloc/Allocator.h>
#include <Geometry/BBox.h>

class LabelSurface : public Module {
    SurfaceIPort* iport;
    SurfaceOPort* oport;
    TCLint numberf;
    TCLstring namef;
    int generation;
    Array1<clString> origNames;
public:
    LabelSurface(const clString& id);
    LabelSurface(const LabelSurface&, int deep);
    virtual ~LabelSurface();
    virtual Module* clone(int deep);
    virtual void execute();
};

extern "C" {
Module* make_LabelSurface(const clString& id)
{
    return new LabelSurface(id);
}
}

//static clString module_name("LabelSurface");

LabelSurface::LabelSurface(const clString& id)
: Module("LabelSurface", id, Filter), generation(-1),
  numberf("numberf", id, this), namef("namef", id, this)
{
    // Create the input ports
    iport=new SurfaceIPort(this, "In Surf", SurfaceIPort::Atomic);
    add_iport(iport);
    // Create the output port
    oport=new SurfaceOPort(this, "Out Surf", SurfaceIPort::Atomic);
    add_oport(oport);
}

LabelSurface::LabelSurface(const LabelSurface& copy, int deep)
: Module(copy, deep), generation(-1),
  numberf("numberf", id, this), namef("namef", id, this)
{
    NOT_FINISHED("LabelSurface::LabelSurface");
}

LabelSurface::~LabelSurface()
{
}

Module* LabelSurface::clone(int deep)
{
    return new LabelSurface(*this, deep);
}

void LabelSurface::execute()
{
    SurfaceHandle iSurf;

    if(!iport->get(iSurf))
	return;
    SurfTree* st=iSurf->getSurfTree();
    TriSurface *ts=iSurf->getTriSurface();
    if (st) {
      if (st->generation != generation) {
	if (st->surfNames.size() != st->surfEls.size()) 
	  st->surfNames.resize(st->surfEls.size());
	origNames = st->surfNames;
      }
      int fnum=numberf.get();
      clString fname=namef.get();

      st->surfNames=origNames;
      
      if (fnum>0 && fnum<st->surfNames.size() && st->surfNames[fnum] != fname) {
	cerr << "Added label: "<<fname<<" to surface number: "<<fnum<<"\n";
	st->surfNames[fnum]=fname;
      }
    } else if (ts) {
      ts->name = namef.get();
    }
    oport->send(iSurf);
}	

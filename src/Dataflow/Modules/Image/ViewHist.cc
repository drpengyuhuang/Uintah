//static char *id="@(#) $Id"

/*
 *  ViewHist.cc:  Histogram Viewer
 *
 *  Written by:
 *    Scott Morris
 *    October 1997
 */

#include <Core/Containers/Array1.h>
#include <Dataflow/Network/Module.h>
#include <Dataflow/Ports/GeometryPort.h>
#include <Dataflow/Ports/ScalarFieldPort.h>
#include <Core/Datatypes/ScalarFieldRG.h>
#include <Dataflow/Ports/ColorMapPort.h>
#include <Core/Geom/GeomGrid.h>
#include <Core/Geom/GeomGroup.h>
#include <Core/Geom/GeomLine.h>
#include <Core/Geom/Material.h>
#include <Core/Geometry/Point.h>
#include <Core/Math/MinMax.h>
#include <Core/Malloc/Allocator.h>
#include <Core/TclInterface/TCLvar.h>
#include <Core/Thread/Parallel.h>
#include <Core/Thread/Thread.h>
#include <iostream>
using std::cerr;
#include <math.h>


namespace SCIRun {



class ViewHist : public Module {
   ScalarFieldIPort *inscalarfield;
   ScalarFieldOPort *outscalarfield;
   int gen;

   ScalarFieldRG* newgrid;
   ScalarFieldRG*  rg;

   double min,max;       // Max and min values of the scalarfield


   int np; // number of proccesors
  
public:
   ViewHist(const clString& id);
   virtual ~ViewHist();
   virtual void execute();

//   void tcl_command( TCLArgs&, void *);

   void do_ViewHist(int proc);

};

extern "C" Module* make_ViewHist(const clString& id)
{
   return scinew ViewHist(id);
}

//static clString module_name("ViewHist");

ViewHist::ViewHist(const clString& id)
: Module("ViewHist", id, Filter)
{
    // Create the input ports
    // Need a scalar field
  
    inscalarfield = scinew ScalarFieldIPort( this, "Scalar Field",
					ScalarFieldIPort::Atomic);
    add_iport( inscalarfield);

    // Create the output port
    outscalarfield = scinew ScalarFieldOPort( this, "Scalar Field",
					ScalarFieldIPort::Atomic);
    add_oport( outscalarfield);
    newgrid=new ScalarFieldRG;
}

ViewHist::~ViewHist()
{
}

void ViewHist::do_ViewHist(int proc)    
{
  int start = (rg->grid.dim2())*proc/np;
  int end   = (proc+1)*(rg->grid.dim2())/np;

  for(int x=start; x<end; x++) {
    int y;
    for(y=0; y<rg->grid.dim1(); y++)
     newgrid->grid(y,x,0)=0;
    for(y=0; y<((rg->grid(0,x,0)/max)*500); y++) {
      newgrid->grid(y,x,0)=255;
    }
  }
}

void ViewHist::execute()
{
    // get the scalar field...if you can

    ScalarFieldHandle sfield;
    if (!inscalarfield->get( sfield ))
	return;

    rg=sfield->getRG();
    
    if(!rg){
      cerr << "ViewHist cannot handle this field type\n";
      return;
    }
    gen=rg->generation;
    
    if (gen!=newgrid->generation){
    //  newgrid=new ScalarFieldRGint;
      // New input
    }
    newgrid=new ScalarFieldRG;

    rg->compute_minmax();
    rg->get_minmax(min,max);

    cerr << "ViewHist min/max : " << min << " " << max << "\n";
    
    newgrid->resize(500,rg->grid.dim2(),1);

    np = Thread::numProcessors();    
      
    Thread::parallel(Parallel<ViewHist>(this, &ViewHist::do_ViewHist),
		     np, true);

    outscalarfield->send( newgrid );
}

/*
void ViewHist::tcl_command(TCLArgs& args, void* userdata)
{
  Module::tcl_command(args, userdata);
}
*/

} // End namespace SCIRun


//static char *id="@(#) $Id"

/*
 *  Unop.cc:  Unary Operations on ScalarFields
 *
 *  Written by:
 *    Scott Morris
 *    November 1997
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



class Unop : public Module {
  ScalarFieldIPort *inscalarfield;
  ScalarFieldOPort *outscalarfield;
  int gen;
  
  ScalarFieldRG* newgrid;
  ScalarFieldRG* rg;
    
  int mode;             // The number of the operation being preformed
  double min,max;       // Max and min values of the scalarfield
  int *nonzero;
  
  TCLstring funcname;
  
  int np; // number of proccesors
  
public:
  Unop(const clString& id);
  virtual ~Unop();
  virtual void execute();
  
  void tcl_command( TCLArgs&, void *);
  
  void do_op(int proc);
};

extern "C" Module* make_Unop(const clString& id)
{
   return scinew Unop(id);
}

//static clString module_name("Unop");

Unop::Unop(const clString& id)
: Module("Unop", id, Filter),  funcname("funcname",id,this)
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
    mode=0;
}

Unop::~Unop()
{
}

void Unop::do_op(int proc)    // Do the operations in paralell
{
  int start = (newgrid->grid.dim2())*proc/np;
  int end   = (proc+1)*(newgrid->grid.dim2())/np;

  for(int z=0; z<newgrid->grid.dim3(); z++) 
    for(int x=start; x<end; x++) {
      for(int y=0; y<newgrid->grid.dim1(); y++) {
	switch (mode) {
	case 0:
	  if (rg->grid(y,x,z)>0)
	    newgrid->grid(y,x,z) = rg->grid(y,x,z); else
	      newgrid->grid(y,x,z) = - rg->grid(y,x,z);
	  break;
	case 1:  
	  newgrid->grid(y,x,z) = -rg->grid(y,x,z);
	  break;
	case 2:  
	  newgrid->grid(y,x,z) = max-rg->grid(y,x,z) + min;
	  break;
	case 3:
	  newgrid->grid(y,x,z) = rg->grid(y,x,z);
	  break;
	case 4:
	  newgrid->grid(y,x,z) = (rg->grid(y,x,0) + rg->grid(y,x,1) +
	    rg->grid(y,x,2)) / 3;
	  break;
	case 5:
	  newgrid->grid(y,x,z) = rg->grid(y,x,z)*rg->grid(y,x,z);
	  break;
	case 6:
	  newgrid->grid(y,x,z) = sqrt(rg->grid(y,x,z));
	  break;
	case 7:
	  newgrid->grid(y,x,z) = atan(rg->grid(y,x,z));
	  break;
	case 8:
	  if (rg->grid(y,x,z)!=0) nonzero[proc]++;
	  newgrid->grid(y,x,z)=rg->grid(y,x,z);
	  break;	    
      	case 9:
	  if ((x<rg->grid.dim2()) && (y<rg->grid.dim1()))
	    newgrid->grid(y,x,z)=rg->grid(y,x,z); else
	      newgrid->grid(y,x,z)=0;
	}
      }
    }
}

void Unop::execute()
{
    // get the scalar field...if you can

    ScalarFieldHandle sfield;
    if (!inscalarfield->get( sfield ))
	return;

    rg=sfield->getRG();
    
    if(!rg){
      cerr << "Unop cannot handle this field type\n";
      return;
    }

    newgrid=new ScalarFieldRG;
    
    np = Thread::numProcessors();

    
    clString ft(funcname.get());

    if (ft=="Abs") mode=0;
    if (ft=="Negative") mode=1;
    if (ft=="Invert") mode=2;
    if (ft=="Max/Min") mode=3;
    if (ft=="Grayscale") mode=4;
    if (ft=="A^2") mode=5;
    if (ft=="Sqrt(A)") mode=6;
    if (ft=="arctan") mode=7;
    if (ft=="nonzero") {
      mode=8;
      nonzero = new int[np];
      for (int i=0; i<np; i++) nonzero[i]=0;
    }
    if (ft=="resize-to-power-of-2") {
      mode = 9;
      int pow = 2;
      while (pow<Max(rg->grid.dim1(),rg->grid.dim2()))
	pow*=2;
      newgrid->resize(pow,pow,rg->grid.dim3());
    } else
      if (mode==4)
	newgrid->resize(rg->grid.dim1(),rg->grid.dim2(),1); else
	  newgrid->resize(rg->grid.dim1(),rg->grid.dim2(),rg->grid.dim3());
    
    rg->compute_minmax();
    rg->get_minmax(min,max);

    cerr << "min/max : " << min << " " << max << "\n";
    
    if ((mode==4) && (rg->grid.dim3()!=3))
      cerr << "Can't convert non-RGB Image to grayscale.\n"; else
	  Thread::parallel(Parallel<Unop>(this, &Unop::do_op),
			   np, true);

    if (mode==8) {
      int nz=0;
      for (int i=0; i<np; i++)
	nz+=nonzero[i];
      cerr << "Non-Zero Pixels : " << nz << ".\n";
    }
    outscalarfield->send( newgrid );
}


void Unop::tcl_command(TCLArgs& args, void* userdata)
{
  Module::tcl_command(args, userdata);
}

} // End namespace SCIRun


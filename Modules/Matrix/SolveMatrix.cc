/*
 *  SolveMatrix.cc:  Unfinished modules
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */


#include <config.h>

#ifdef SCI_SPARSELIB
#include "comprow_double.h"        //compressed row matrix storage
#include "iotext_double.h"        //matrix, vector input- output


#include "mvm.h"           //matrix definitions
#include "mvv.h"           //vector definitions
#include "mvblasd.h"       //vector multiplication BLASD


#include "icpre_double.h"         //preconditionars
#include "diagpre_double.h"
#include "ilupre_double.h"


#include "cg.h"                  //iterative IML  methods
#include "bicg.h"
#include "qmr.h"
#include "cgs.h"
#include "bicgstab.h"
#include "ir.h"
#include "gmres.h"
#endif



#include <Classlib/NotFinished.h>
#include <Dataflow/Module.h>
#include <Datatypes/ColumnMatrixPort.h>
#include <Datatypes/MatrixPort.h>
#include <Datatypes/SurfacePort.h>
#include <Geometry/Point.h>
#include <Malloc/Allocator.h>
#include <TCL/TCLvar.h>
#include <strstream.h>

#include <Multitask/Task.h>

struct CGData;

class SolveMatrix : public Module {
    MatrixIPort* matrixport;
    ColumnMatrixIPort* rhsport;
    ColumnMatrixOPort* solport;
    ColumnMatrixHandle solution;
  
#ifdef SCI_SPARSELIB
    void conjugate_gradient(Matrix*, ColumnMatrix&, ColumnMatrix&,int flag);
    void quasi_minimal_res(Matrix*, ColumnMatrix&, ColumnMatrix&,int flag);
    void bi_conjugate_gradient(Matrix*, ColumnMatrix&, ColumnMatrix&,int flag);
    void bi_conjugate_gradient_stab(Matrix*, ColumnMatrix&, ColumnMatrix&,int flag);
    void conj_grad_squared(Matrix*, ColumnMatrix&, ColumnMatrix&,int flag);
    void gen_min_res_iter(Matrix*, ColumnMatrix&, ColumnMatrix&,int flag);
    void richardson_iter(Matrix*, ColumnMatrix&, ColumnMatrix&,int flag);
#endif

    void jacobi_sci(Matrix*,ColumnMatrix& , ColumnMatrix&);
    void conjugate_gradient_sci(Matrix*,ColumnMatrix&, ColumnMatrix&);


  
    void append_values(int niter, const Array1<double>& errlist,
		       int& last_update, const Array1<int>& targetidx,
		       const Array1<double>& targetlist,
		       int& last_errupdate);
public:
    void parallel_conjugate_gradient(CGData*, int proc);
    SolveMatrix(const clString& id);
    SolveMatrix(const SolveMatrix&, int deep);
    virtual ~SolveMatrix();
    virtual Module* clone(int deep);
    virtual void execute();

    TCLdouble target_error;
    TCLdouble flops;
    TCLdouble floprate;
    TCLdouble memrefs;
    TCLdouble memrate;
    TCLdouble orig_error;
    TCLdouble current_error;
    TCLstring method;
    TCLstring precond;
    TCLint iteration;
    TCLint maxiter;
    TCLint use_previous_soln;
    TCLvarint emit_partial;
    TCLstring status;
};

extern "C" {
Module* make_SolveMatrix(const clString& id)
{
    return scinew SolveMatrix(id);
}
}


SolveMatrix::SolveMatrix(const clString& id)
: Module("SolveMatrix", id, Filter), target_error("target_error", id, this),
  flops("flops", id, this), floprate("floprate", id, this),
  memrefs("memrefs", id, this), memrate("memrate", id, this),
  orig_error("orig_error", id, this), current_error("current_error", id, this),
  method("method", id, this),precond("precond",id,this), iteration("iteration", id, this),
  maxiter("maxiter", id, this),
  use_previous_soln("use_previous_soln", id, this),
  emit_partial("emit_partial", id, this),status("status",id,this)
{
    matrixport=scinew MatrixIPort(this, "Matrix", MatrixIPort::Atomic);
    add_iport(matrixport);
    rhsport=scinew ColumnMatrixIPort(this, "RHS", ColumnMatrixIPort::Atomic);
    add_iport(rhsport);

    solport=scinew ColumnMatrixOPort(this, "Solution", ColumnMatrixIPort::Atomic);
    add_oport(solport);
}

SolveMatrix::SolveMatrix(const SolveMatrix& copy, int deep)
: Module(copy, deep), target_error("target_error", id, this),
  flops("flops", id, this), floprate("floprate", id, this),
  memrefs("memrefs", id, this), memrate("memrate", id, this),
  orig_error("orig_error", id, this), current_error("current_error", id, this),
  method("method", id, this),precond("precond",id,this), iteration("iteration", id, this),
  maxiter("maxiter", id, this),
  use_previous_soln("use_previous_soln", id, this),
  emit_partial("emit_partial", id, this),status("status",id,this)
{
    NOT_FINISHED("SolveMatrix::SolveMatrix");
}

SolveMatrix::~SolveMatrix()
{
}

Module* SolveMatrix::clone(int deep)
{
    return scinew SolveMatrix(*this, deep);
}


void SolveMatrix::execute()
{
#ifdef SCI_SPARSELIB
 int flag = 1;
#endif
  
    MatrixHandle matrix;
    if(!matrixport->get(matrix))
	return;	
    ColumnMatrixHandle rhs;
    if(!rhsport->get(rhs))
	return;
    if(use_previous_soln.get() && solution.get_rep() && solution->nrows() == rhs->nrows()){
	solution.detach();
    } else {
	solution=scinew ColumnMatrix(rhs->nrows());
	solution->zero();
    }

    int size=matrix->nrows();
    if(matrix->ncols() != size){
	error(clString("Matrix should be square, but is ")
	      +to_string(size)+" x "+to_string(matrix->ncols()));
	return;
    }
    if(rhs->nrows() != size){
	error(clString("Matrix size mismatch"));
	return;
    }


  
#ifdef SCI_SPARSELIB
  clString pre=precond.get();
      if(pre == "Diag_P") flag = 1;
      else if(pre == "IC_P") flag = 2;
      else if(pre == "ILU_P") flag = 3;
#endif

    clString meth=method.get();
    if(0){
#ifdef SCI_SPARSELIB
    } else if(meth == "conjugate_gradient"){
	rhs.detach();
	conjugate_gradient(matrix.get_rep(),
			   *solution.get_rep(), *rhs.get_rep(),flag);
	solport->send(solution);
    } else if(meth == "quasi_minimal_res"){
	quasi_minimal_res(matrix.get_rep(),
	       *solution.get_rep(), *rhs.get_rep(),flag);
	solport->send(solution);
    } else if(meth == "bi_conjugate_gradient"){
	bi_conjugate_gradient(matrix.get_rep(),
	       *solution.get_rep(), *rhs.get_rep(),flag);
	solport->send(solution);
    } else if(meth == "bi_conjugate_gradient_stab"){
	bi_conjugate_gradient_stab(matrix.get_rep(),
	       *solution.get_rep(), *rhs.get_rep(),flag);
	solport->send(solution);

   } else if(meth == "conj_grad_squared"){
	conj_grad_squared(matrix.get_rep(),
	       *solution.get_rep(), *rhs.get_rep(),flag);
        solport->send(solution);
  } else if(meth == "gen_min_res_iter"){
	gen_min_res_iter(matrix.get_rep(),
	       *solution.get_rep(), *rhs.get_rep(),flag);
	solport->send(solution);

   } else if(meth == "richardson_iter"){
	richardson_iter(matrix.get_rep(),
	       *solution.get_rep(), *rhs.get_rep(),flag);
        solport->send(solution);
	
#endif
   } else if(meth == "conjugate_gradient_sci"){
	conjugate_gradient_sci(matrix.get_rep(),
	       *solution.get_rep(), *rhs.get_rep());
        solport->send(solution);


   } else if(meth == "jacoby_sci"){
	jacobi_sci(matrix.get_rep(),
	       *solution.get_rep(), *rhs.get_rep());
        solport->send(solution);


	
    }else {
	error(clString("Unknown solution method: ")+meth);
    }

}




void SolveMatrix::append_values(int niter, const Array1<double>& errlist,
				int& last_update,
				const Array1<int>& targetidx,
				const Array1<double>& targetlist,
				int& last_errupdate)
{
    char buf[10000];
    ostrstream str(buf, 10000);
    str << id << " append_graph " << niter << " \"";
    int i;
    for(i=last_update;i<errlist.size();i++){
	str << i << " " << errlist[i] << " ";
    }
    str << "\" \"";
    for(i=last_errupdate;i<targetidx.size();i++){
	str << targetidx[i] << " " << targetlist[i] << " ";
    }
    str << "\" ; update idletasks" << '\0';
    TCL::execute(str.str());
    last_update=errlist.size();
    last_errupdate=targetidx.size();
}


#ifdef SCI_SPARSELIB

//********************** IML++  **************************************************

void SolveMatrix::conjugate_gradient(Matrix* matrix,
				    ColumnMatrix& lhs, ColumnMatrix& rhs,int flag)
{
int result;  
int size = matrix->nrows();
int non_zero =  matrix->get_row()[size];


int maxit = maxiter.get();
double tol = target_error.get();
double x_init = 0.0;


status.set("Running"); 
  TCL::execute("update idletasks");
iteration.set(0); 
current_error.set(0);
  TCL::execute("update idletasks");


VECTOR_double b(rhs.get_rhs(),size);


VECTOR_double  x(size,x_init);

for(int i=0;i<size;i++)
   x[i] = lhs[i];



CompRow_Mat_double A(size,size,non_zero,matrix->get_val(),matrix->get_row(),matrix->get_col()); 
 

if (flag == 1){
     DiagPreconditioner_double D(A);
     result = CG(A,x,b,D,maxit,tol);  //IML++ solver 
}

else if(flag == 2){
     ICPreconditioner_double D(A);
     result = CG(A,x,b,D,maxit,tol);  //IML++ solver  
}

else if(flag ==3){
   CompRow_ILUPreconditioner_double D(A);
    result = CG(A,x,b,D,maxit,tol);  //IML++ solver
}


for(i=0;i<size;i++)
  lhs[i] = x[i];

//lhs.put_lhs(&x[0]);



if(result == 0)
  status.set("Done");
else
  status.set("Failed to Converge");

iteration.set(maxit);
current_error.set(tol);	      
 TCL::execute("update idletasks");

}


void SolveMatrix::bi_conjugate_gradient(Matrix* matrix,
				    ColumnMatrix& lhs, ColumnMatrix& rhs,int flag)
{
int result; 
int size = matrix->nrows();

int non_zero =  matrix->get_row()[size];

int maxit = maxiter.get();
double tol = target_error.get();
double x_init = 0.0;

status.set("Running");
  TCL::execute("update idletasks");
iteration.set(0);
current_error.set(0);
 TCL::execute("update idletasks");


VECTOR_double b(rhs.get_rhs(),size);

VECTOR_double  x(size,x_init);

for(int i=0;i<size;i++)
   x[i] = lhs[i];


CompRow_Mat_double A(size,size,non_zero,matrix->get_val(),matrix->get_row(),matrix->get_col()); 


if (flag == 1){
     DiagPreconditioner_double D(A);
     result = BiCG(A,x,b,D,maxit,tol);  //IML++ solver 
}

else if(flag == 2){
     ICPreconditioner_double D(A);
     result = BiCG(A,x,b,D,maxit,tol);  //IML++ solver  
}

else if(flag ==3){
   CompRow_ILUPreconditioner_double D(A);
    result = BiCG(A,x,b,D,maxit,tol);  //IML++ solver
}

//lhs.put_lhs(&x[0]);

for(i=0;i<size;i++)
   lhs[i] = x[i];

if(result == 0)
  status.set("Done");
else
  status.set("Failed to Converge");

iteration.set(maxit);
current_error.set(tol);	   
 TCL::execute("update idletasks");

}


void SolveMatrix::quasi_minimal_res(Matrix* matrix,
				    ColumnMatrix& lhs, ColumnMatrix& rhs,int flag)
{
int result; 
int size = matrix->nrows();

int non_zero =  matrix->get_row()[size];

int maxit = maxiter.get();
double tol = target_error.get();
double x_init = 0.0;


status.set("Running");
  TCL::execute("update idletasks");
iteration.set(0);
current_error.set(0);
 TCL::execute("update idletasks");

VECTOR_double b(rhs.get_rhs(),size);

VECTOR_double  x(size,x_init);

for(int i=0;i<size;i++)
   x[i] = lhs[i];

CompRow_Mat_double A(size,size,non_zero,matrix->get_val(),matrix->get_row(),matrix->get_col()); 


if (flag == 1){
     DiagPreconditioner_double D(A);
     result = QMR(A,x,b,D,D,maxit,tol);  //IML++ solver 
}

else if(flag == 2){
     ICPreconditioner_double D(A);
     result = QMR(A,x,b,D,D,maxit,tol);  //IML++ solver  
}

else if(flag ==3){
   CompRow_ILUPreconditioner_double D(A);
    result = QMR(A,x,b,D,D,maxit,tol);  //IML++ solver
}

//lhs.put_lhs(&x[0]);

for(i=0;i<size;i++)
   lhs[i] = x[i];


if(result == 0)
  status.set("Done");
else
  status.set("Failed to Converge");

iteration.set(maxit);
current_error.set(tol);	   

 TCL::execute("update idletasks");

}



void SolveMatrix::bi_conjugate_gradient_stab(Matrix* matrix,
				    ColumnMatrix& lhs, ColumnMatrix& rhs,int flag)
{
int result; 
int size = matrix->nrows();

int non_zero =  matrix->get_row()[size];
int maxit = maxiter.get();
double tol = target_error.get();
double x_init = 0.0;

status.set("Running");
 TCL::execute("update idletasks");
iteration.set(0);
current_error.set(0);
 TCL::execute("update idletasks");

VECTOR_double b(rhs.get_rhs(),size);

VECTOR_double  x(size,x_init);

for(int i=0;i<size;i++)
   x[i] = lhs[i];

CompRow_Mat_double A(size,size,non_zero,matrix->get_val(),matrix->get_row(),matrix->get_col()); 



if (flag == 1){
     DiagPreconditioner_double D(A);
     result = BiCGSTAB(A,x,b,D,maxit,tol);  //IML++ solver 
}

else if(flag == 2){
     ICPreconditioner_double D(A);
     result = BiCGSTAB(A,x,b,D,maxit,tol);  //IML++ solver  
}

else if(flag ==3){
   CompRow_ILUPreconditioner_double D(A);
    result = BiCGSTAB(A,x,b,D,maxit,tol);  //IML++ solver
}

//lhs.put_lhs(&x[0]);

for(i=0;i<size;i++)
   lhs[i] = x[i];


if(result == 0)
  status.set("Done");
else
  status.set("Failed to Converge");

iteration.set(maxit);
current_error.set(tol);	   

 TCL::execute("update idletasks");

}


void SolveMatrix::conj_grad_squared(Matrix* matrix,
				    ColumnMatrix& lhs, ColumnMatrix& rhs,int flag)
{
int result; 
int size = matrix->nrows();

int non_zero =  matrix->get_row()[size];
int maxit = maxiter.get();
double tol = target_error.get();
double x_init = 0.0;

status.set("Running");
TCL::execute("update idletasks");
iteration.set(0);
current_error.set(0);
 TCL::execute("update idletasks");

VECTOR_double b(rhs.get_rhs(),size);
VECTOR_double  x(size,x_init);

for(int i=0;i<size;i++)
   x[i] = lhs[i];

CompRow_Mat_double A(size,size,non_zero,matrix->get_val(),matrix->get_row(),matrix->get_col()); 


if (flag == 1){
     DiagPreconditioner_double D(A);
     result = CGS(A,x,b,D,maxit,tol);  //IML++ solver 
}

else if(flag == 2){
     ICPreconditioner_double D(A);
     result = CGS(A,x,b,D,maxit,tol);  //IML++ solver  
}

else if(flag ==3){
   CompRow_ILUPreconditioner_double D(A);
    result = CGS(A,x,b,D,maxit,tol);  //IML++ solver
}

//lhs.put_lhs(&x[0]);

for(i=0;i<size;i++)
   lhs[i] = x[i];

if(result == 0)
  status.set("Done");
else
  status.set("Failed to Converge");

iteration.set(maxit);
current_error.set(tol);	   
 TCL::execute("update idletasks");
}

 
void SolveMatrix::gen_min_res_iter(Matrix* matrix,
				    ColumnMatrix& lhs, ColumnMatrix& rhs,int flag)
{
int result; 
int restart = 32;
int size = matrix->nrows();

int non_zero =  matrix->get_row()[size];
int maxit = maxiter.get();
double tol = target_error.get();
double x_init = 0.0;

status.set("Running");
TCL::execute("update idletasks");
iteration.set(0);
current_error.set(0);
 TCL::execute("update idletasks");

VECTOR_double b(rhs.get_rhs(),size);
VECTOR_double  x(size,x_init);

MATRIX_double H(restart+1,restart,0.0);

for(int i=0;i<size;i++)
   x[i] = lhs[i];

CompRow_Mat_double A(size,size,non_zero,matrix->get_val(),matrix->get_row(),matrix->get_col()); 


if (flag == 1){
     DiagPreconditioner_double D(A);
     result = GMRES(A,x,b,D,H,restart,maxit,tol);  //IML++ solver 
}

else if(flag == 2){
     ICPreconditioner_double D(A);
     result = GMRES(A,x,b,D,H,restart,maxit,tol);  //IML++ solver  
}

else if(flag ==3){
   CompRow_ILUPreconditioner_double D(A);
    result = GMRES(A,x,b,D,H,restart,maxit,tol);  //IML++ solver
}

//lhs.put_lhs(&x[0]);

for(i=0;i<size;i++)
   lhs[i] = x[i];

if(result == 0)
  status.set("Done");
else
  status.set("Failed to Converge");

iteration.set(maxit);
current_error.set(tol);	   
 TCL::execute("update idletasks");

}


void SolveMatrix::richardson_iter(Matrix* matrix,
				    ColumnMatrix& lhs, ColumnMatrix& rhs,int flag)
{
int result; 
int size = matrix->nrows();

int non_zero =  matrix->get_row()[size];
int maxit = maxiter.get();
double tol = target_error.get();
double x_init = 0.0;

status.set("Running");
TCL::execute("update idletasks");
iteration.set(0);
current_error.set(0);
 TCL::execute("update idletasks");

VECTOR_double b(rhs.get_rhs(),size);
VECTOR_double  x(size,x_init);

for(int i=0;i<size;i++)
   x[i] = lhs[i];

CompRow_Mat_double A(size,size,non_zero,matrix->get_val(),matrix->get_row(),matrix->get_col()); 


if (flag == 1){
     DiagPreconditioner_double D(A);
     result = IR(A,x,b,D,maxit,tol);  //IML++ solver 
}

else if(flag == 2){
     ICPreconditioner_double D(A);
     result = IR(A,x,b,D,maxit,tol);  //IML++ solver  
}

else if(flag ==3){
   CompRow_ILUPreconditioner_double D(A);
    result = IR(A,x,b,D,maxit,tol);  //IML++ solver
}

//lhs.put_lhs(&x[0]);

for(i=0;i<size;i++)
   lhs[i] = x[i];

if(result == 0)
  status.set("Done");
else
  status.set("Failed to Converge");

iteration.set(maxit);
current_error.set(tol);	   
 TCL::execute("update idletasks");

}
//**********************End IML++ *********************************************


#endif 



void SolveMatrix::jacobi_sci(Matrix* matrix,
			 ColumnMatrix& lhs, ColumnMatrix& rhs)
{
    int size=matrix->nrows();

    int flop=0;
    int memref=0;
    int gflop=0;
    int grefs=0;
    flops.set(flop);
    floprate.set(0);
    memrefs.set(memref);
    memrate.set(0);

    iteration.set(0);
    
    ColumnMatrix invdiag(size);
    // We should try to do a better job at preconditioning...
    int i;

    for(i=0;i<size;i++){
	invdiag[i]=1./matrix->get(i,i);
    }
    flop+=size;
    memref=2*size*sizeof(double);

    ColumnMatrix Z(size);
    matrix->mult(lhs, Z, flop, memref);

    Sub(Z, Z, rhs, flop, memref);
    double bnorm=rhs.vector_norm(flop, memref);
    double err=Z.vector_norm(flop, memref)/bnorm;

    orig_error.set(err);
    current_error.set(err);

    int niter=0;
    int toomany=maxiter.get();
    if(toomany == 0)
	toomany=2*size;
    double max_error=target_error.get();

    double time=timer.time();
    flops.set(gflop*1.e9+flop);
    floprate.set((gflop*1.e3+flop*1.e-6)/time);
    memrefs.set(grefs*1.e9+memref);
    memrate.set((grefs*1.e3+memref*1.e-6)/time);
    gflop+=flop/1000000000;
    flop=flop%1000000000;
    grefs+=memref/1000000000;
    memref=memref%1000000000;
    
    TCL::execute(id+" reset_graph");
    Array1<double> errlist;
    errlist.add(err);
    int last_update=0;

    Array1<int> targetidx;
    Array1<double> targetlist;
    int last_errupdate=0;
    targetidx.add(0);
    targetlist.add(max_error);

    append_values(1, errlist, last_update, targetidx, targetlist, last_errupdate);

    double log_orig=log(err);
    double log_targ=log(max_error);
    while(niter < toomany){
	niter++;

	double new_error;
	if(get_tcl_doublevar(id, "target_error", new_error)
	   && new_error != max_error){
	    targetidx.add(niter);
	    targetlist.add(max_error);
	    max_error=new_error;
	}
	targetidx.add(niter);
	targetlist.add(max_error);
	if(err < max_error)
	    break;
	if(err > 10){
	    error("Solution not converging!");
	    break;
	}

	Mult(Z, invdiag, Z, flop, memref);
	ScMult_Add(lhs, 1, lhs, Z, flop, memref);
//	Sub(lhs, lhs, Z, flop, memref);

	matrix->mult(lhs, Z, flop, memref);
	Sub(Z, rhs, Z, flop, memref);
	err=Z.vector_norm(flop, memref)/bnorm;

	errlist.add(err);

	gflop+=flop/1000000000;
	flop=flop%1000000000;
	grefs+=memref/1000000000;
	memref=memref%1000000000;

	if(niter == 1 || niter == 5 || niter%10 == 0){
	    iteration.set(niter);
	    current_error.set(err);
	    double time=timer.time();
	    flops.set(gflop*1.e9+flop);
	    floprate.set((gflop*1.e3+flop*1.e-6)/time);
	    memrefs.set(grefs*1.e9+memref);
	    memrate.set((grefs*1.e3+memref*1.e-6)/time);

	    append_values(niter, errlist, last_update, targetidx, targetlist, last_errupdate);

	    double progress=(log_orig-log(err))/(log_orig-log_targ);
	    update_progress(progress);
#if 0
	    if(emit_partial.get() && niter%50 == 0)
		solport->send_intermediate(rhs.clone());
#endif
	}
    }
    iteration.set(niter);
    current_error.set(err);

    time=timer.time();
    flops.set(gflop*1.e9+flop);
    floprate.set((gflop*1.e3+flop*1.e-6)/time);
    memrefs.set(grefs*1.e9+memref);
    memrate.set((grefs*1.e3+memref*1.e-6)/time);

    TCL::execute(id+" finish_graph");
    append_values(niter, errlist, last_update, targetidx, targetlist, last_errupdate);
}

struct PStats {
    int flop;
    int memref;
    int gflop;
    int grefs;
    int pad[28];
};

struct Result {
    double r;
    double pad[15];
};

struct CGData {
    SolveMatrix* module;
    WallClockTimer* timer;
    ColumnMatrix* rhs;
    ColumnMatrix* lhs;
    Matrix* mat;
    ColumnMatrix* diag;
    int niter;
    int toomany;
    ColumnMatrix* Z;
    ColumnMatrix* R;
    ColumnMatrix* P;
    double max_error;
    Barrier barrier;
    int np;
    Result* res1;
    Result* res2;
    Result* res3;
    PStats* stats;
    double err;
    double bnorm;
};


void do_parallel_conjugate_gradient(void* d, int processor)
{
    CGData* data=(CGData*)d;
    data->module->parallel_conjugate_gradient(data, processor);
}



void SolveMatrix::conjugate_gradient_sci(Matrix* matrix,
				    ColumnMatrix& lhs, ColumnMatrix& rhs)
{
    cerr << "cg started\n";
    CPUTimer timer;
    timer.start();
    int np=Task::nprocessors();
    if (np>4) np/=2;	// being nice -- only using half the processors...
    cerr << "np=" << np << endl;
    CGData* data=new CGData;
    data->module=this;
    data->np=np;
    data->rhs=&rhs;
    data->lhs=&lhs;
    data->mat=matrix;
    data->timer=new WallClockTimer;
    data->stats=new PStats[data->np];
    Task::multiprocess(data->np, do_parallel_conjugate_gradient, data);
    delete data->timer;
    delete data->stats;
    timer.stop();
    cerr << "cg done: " << timer.time() << " seconds\n";
}

void SolveMatrix::parallel_conjugate_gradient(CGData* data, int processor)
{
    Matrix* matrix=data->mat;
    PStats* stats=&data->stats[processor];
    int size=matrix->nrows();

    int beg=processor*size/data->np;
    int end=(processor+1)*size/data->np;
    stats->flop=0;
    stats->memref=0;
    stats->gflop=0;
    stats->grefs=0;
    Array1<int> targetidx;
    Array1<double> targetlist;
    Array1<double> errlist;

    int last_update=0;

    int last_errupdate=0;

    if(processor == 0){
        data->timer->clear();
        data->timer->start();
        flops.set(0);
        floprate.set(0);
        memrefs.set(0);
        memrate.set(0);
        iteration.set(0);

        data->diag=new ColumnMatrix(size);
        // We should try to do a better job at preconditioning...
        int i;

        for(i=0;i<size;i++){
            ColumnMatrix& diag=*data->diag;
            diag[i]=1./matrix->get(i,i);
        }
        stats->flop+=size;
        stats->memref+=2*size*sizeof(double);
        data->R=new ColumnMatrix(size);
        ColumnMatrix& R=*data->R;
        ColumnMatrix& lhs=*data->lhs;
        matrix->mult(lhs, R, stats->flop, stats->memref);


        ColumnMatrix& rhs=*data->rhs;
        Sub(R, rhs, R, stats->flop, stats->memref);
        data->bnorm=rhs.vector_norm(stats->flop, stats->memref);

        data->Z=new ColumnMatrix(size);
        ColumnMatrix& Z=*data->Z;
        matrix->mult(R, Z, stats->flop, stats->memref);

        data->P=new ColumnMatrix(size);
        //ColumnMatrix& P=*data->P;
        data->err=R.vector_norm(stats->flop, stats->memref)/data->bnorm;
        if(data->err == 0){
            lhs=rhs;
            stats->memref+=2*size*sizeof(double);
            return;
        }

        data->niter=0;
        data->toomany=maxiter.get();
        if(data->toomany == 0)
            data->toomany=2*size;
        data->max_error=target_error.get();
        data->res1=new Result[data->np];
        data->res2=new Result[data->np];
        data->res3=new Result[data->np];

        stats->gflop+=stats->flop/1000000000;
        stats->flop=stats->flop%1000000000;
        stats->grefs+=stats->memref/1000000000;
        stats->memref=stats->memref%1000000000;
        orig_error.set(data->err);
        current_error.set(data->err);

        double time=data->timer->time();
        flops.set(stats->gflop*1.e9+stats->flop);
        floprate.set((stats->gflop*1.e3+stats->flop*1.e-6)/time);
        memrefs.set(stats->grefs*1.e9+stats->memref);
        memrate.set((stats->grefs*1.e3+stats->memref*1.e-6)/time);

        TCL::execute(id+" reset_graph");
        errlist.add(data->err);
        targetidx.add(0);
        targetlist.add(data->max_error);

        append_values(1, errlist, last_update, targetidx, targetlist, last_errupdate);
    }
    double log_orig=log(data->err);
    double log_targ=log(data->max_error);
    data->barrier.wait(data->np);
    double err=data->err;
    double bkden=0;
    while(data->niter < data->toomany){
        if(err < data->max_error)
            break;

        ColumnMatrix& Z=*data->Z;
        ColumnMatrix& P=*data->P;
        if(processor==0){
            data->niter++;
            double new_error;
            if(get_tcl_doublevar(id, "target_error", new_error)
               && new_error != data->max_error){
                targetidx.add(data->niter);
                targetlist.add(data->max_error);
                data->max_error=new_error;
            }
            targetidx.add(data->niter);
            targetlist.add(data->max_error);
        }
        data->barrier.wait(data->np);
        if(err < data->max_error)
            break;

        // Simple Preconditioning...
        ColumnMatrix& diag=*data->diag;
        ColumnMatrix& R=*data->R;
        Mult(Z, R, diag, stats->flop, stats->memref, beg, end);

        // Calculate coefficient bk and direction vectors p and pp
        data->res1[processor].r=Dot(Z, R, stats->flop, stats->memref, beg, end);
        data->barrier.wait(data->np);

        double  bknum=0;
        for(int ii=0;ii<data->np;ii++)
            bknum+=data->res1[ii].r;

        if(data->niter==1){
            Copy(P, Z, stats->flop, stats->memref, beg, end);
        } else {
            double bk=bknum/bkden;
            ScMult_Add(P, bk, P, Z, stats->flop, stats->memref, beg, end);
        }
        data->barrier.wait(data->np);
        // Calculate coefficient ak, new iterate x and new residuals r and rr
        matrix->mult(P, Z, stats->flop, stats->memref, beg, end);
        bkden=bknum;
        data->res2[processor].r=Dot(Z, P, stats->flop, stats->memref, beg, end);
        data->barrier.wait(data->np);

        double akden=0;
        for(ii=0;ii<data->np;ii++)
            akden+=data->res2[ii].r;
        double ak=bknum/akden;
        ColumnMatrix& lhs=*data->lhs;
        ScMult_Add(lhs, ak, P, lhs, stats->flop, stats->memref, beg, end);
        //ColumnMatrix& rhs=*data->rhs;
        ScMult_Add(R, -ak, Z, R, stats->flop, stats->memref, beg, end);

        data->res3[processor].r=R.vector_norm(stats->flop, stats->memref, beg, end)/data->bnorm;
        data->barrier.wait(data->np);
        err=0;
        for(ii=0;ii<data->np;ii++)
            err+=data->res3[ii].r;
        stats->gflop+=stats->flop/1000000000;
        stats->flop=stats->flop%1000000000;
        stats->grefs+=stats->memref/1000000000;
        stats->memref=stats->memref%1000000000;

        if(processor == 0){
            errlist.add(err);

            stats->gflop+=stats->flop/1000000000;
            stats->flop=stats->flop%1000000000;
            stats->grefs+=stats->memref/1000000000;
            stats->memref=stats->memref%1000000000;

            if(data->niter == 1 || data->niter == 10 || data->niter%20 == 0){
                if(data->niter <= 60 || data->niter%60 == 0){
                    iteration.set(data->niter);
                    current_error.set(err);
                    double time=timer.time();
                    flops.set(14*stats->gflop*1.e9+stats->flop);
                    floprate.set(14*(stats->gflop*1.e3+stats->flop*1.e-6)/time);                    memrefs.set(14*stats->grefs*1.e9+stats->memref);
                    memrate.set(14*(stats->grefs*1.e3+stats->memref*1.e-6)/time);
                    append_values(data->niter, errlist, last_update, targetidx,
targetlist, last_errupdate);

                    if(err > 0){
                        double progress=(log_orig-log(err))/(log_orig-log_targ);                        cerr << "err=" << err << endl;
                        cerr << "log_orig=" << log_orig << endl;
                        update_progress(progress);
                    }
                }

                if(data->niter%60 == 0)
                    solport->send_intermediate(lhs.clone());
            }
        }
    }
    if(processor == 0){
        data->niter++;

        iteration.set(data->niter);
        current_error.set(err);
        data->timer->stop();
        double time=data->timer->time();
        flops.set(14*stats->gflop*1.e9+stats->flop);
        floprate.set(14*(stats->gflop*1.e3+stats->flop*1.e-6)/time);
        memrefs.set(14*stats->grefs*1.e9+stats->memref);
        memrate.set(14*(stats->grefs*1.e3+stats->memref*1.e-6)/time);
        cerr << "Done in " << time << " seconds\n";

        TCL::execute(id+" finish_graph");
        append_values(data->niter, errlist, last_update, targetidx, targetlist,
last_errupdate);

    }
}

#ifdef SERIAL
    int size=matrix->nrows();

    int flop=0;
    int memref=0;
    int gflop=0;
    int grefs=0;
    flops.set(flop);
    floprate.set(0);
    memrefs.set(memref);
    memrate.set(0);

    iteration.set(0);


    
    ColumnMatrix diag(size);
    // We should try to do a better job at preconditioning...
    int i;

    for(i=0;i<size;i++){
	diag[i]=1./matrix->get(i,i);
    }
    flop+=size;
    memref=2*size*sizeof(double);

    ColumnMatrix R(size);
    matrix->mult(lhs, R, flop, memref);


    Sub(R, rhs, R, flop, memref);
    double bnorm=rhs.vector_norm(flop, memref);

    ColumnMatrix Z(size);
    matrix->mult(R, Z, flop, memref);

    ColumnMatrix P(size);
    double bkden=0;
    double err=R.vector_norm(flop, memref)/bnorm;
    if(err == 0){
	lhs=rhs;
	return;
    }

    orig_error.set(err);
    current_error.set(err);

    int niter=0;
    int toomany=maxiter.get();
    if(toomany == 0)
	toomany=2*size;
    double max_error=target_error.get();

    double time=timer.time();
    flops.set(gflop*1.e9+flop);
    floprate.set((gflop*1.e3+flop*1.e-6)/time);
    memrefs.set(grefs*1.e9+memref);
    memrate.set((grefs*1.e3+memref*1.e-6)/time);
    gflop+=flop/1000000000;
    flop=flop%1000000000;
    grefs+=memref/1000000000;
    memref=memref%1000000000;
    
    TCL::execute(id+" reset_graph");
    Array1<double> errlist;
    errlist.add(err);
    int last_update=0;

    Array1<int> targetidx;
    Array1<double> targetlist;
    int last_errupdate=0;
    targetidx.add(0);
    targetlist.add(max_error);

    append_values(1, errlist, last_update, targetidx, targetlist, last_errupdate);

    double log_orig=log(err);
    double log_targ=log(max_error);
    while(niter < toomany){
	niter++;

	double new_error;
	if(get_tcl_doublevar(id, "target_error", new_error)
	   && new_error != max_error){
	    targetidx.add(niter);
	    targetlist.add(max_error);
	    max_error=new_error;
	}
	targetidx.add(niter);
	targetlist.add(max_error);
	if(err < max_error)
	    break;

	// Simple Preconditioning...
	Mult(Z, R, diag, flop, memref);	

	// Calculate coefficient bk and direction vectors p and pp
	double bknum=Dot(Z, R, flop, memref);

	if(niter==1){
	    P=Z;
	    memref+=2*sizeof(double);
	} else {
	    double bk=bknum/bkden;
	    ScMult_Add(P, bk, P, Z, flop, memref);
	}
	bkden=bknum;

	// Calculate coefficient ak, new iterate x and new residuals r and rr
	matrix->mult(P, Z, flop, memref);

	double akden=Dot(Z, P, flop, memref);

	double ak=bknum/akden;
	ScMult_Add(lhs, ak, P, lhs, flop, memref);
	ScMult_Add(R, -ak, Z, R, flop, memref);

	err=R.vector_norm(flop, memref)/bnorm;

	errlist.add(err);

	gflop+=flop/1000000000;
	flop=flop%1000000000;
	grefs+=memref/1000000000;
	memref=memref%1000000000;

	if(niter == 1 || niter == 5 || niter%10 == 0){
	    iteration.set(niter);
	    current_error.set(err);
	    double time=timer.time();
	    flops.set(gflop*1.e9+flop);
	    floprate.set((gflop*1.e3+flop*1.e-6)/time);
	    memrefs.set(grefs*1.e9+memref);
	    memrate.set((grefs*1.e3+memref*1.e-6)/time);

	    append_values(niter, errlist, last_update, targetidx, targetlist, last_errupdate);

	    if(err > 0){
		double progress=(log_orig-log(err))/(log_orig-log_targ);
		update_progress(progress);
	    }

#if 0
	    if(emit_partial.get() && niter == 5 || niter%20 == 0)
		solport->send_intermediate(lhs.clone());
#endif
	}
    }
    iteration.set(niter);
    current_error.set(err);

    time=timer.time();
    flops.set(gflop*1.e9+flop);
    floprate.set((gflop*1.e3+flop*1.e-6)/time);
    memrefs.set(grefs*1.e9+memref);
    memrate.set((grefs*1.e3+memref*1.e-6)/time);

    TCL::execute(id+" finish_graph");
    append_values(niter, errlist, last_update, targetidx, targetlist, last_errupdate);
}

#endif

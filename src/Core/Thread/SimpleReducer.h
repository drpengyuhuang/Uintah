
/*
 *  SimpleReducer: A barrier with reduction operations
 *
 *  Written by:
 *   Author: Steve Parker
 *   Department of Computer Science
 *   University of Utah
 *   Date: June 1997
 *
 *  Copyright (C) 1997 SCI Group
 */

#ifndef Core_Thread_SimpleReducer_h
#define Core_Thread_SimpleReducer_h

#include <Core/share/share.h>

#include <Core/Thread/Barrier.h>

namespace SCIRun {

/**************************************
 
CLASS
   SimpleReducer
   
KEYWORDS
   Thread
   
DESCRIPTION
   Perform reduction operations over a set of threads.  Reduction
   operations include things like global sums, global min/max, etc.
   In these operations, a local sum (operation) is performed on each
   thread, and these sums are added together.
   
****************************************/
	class SCICORESHARE SimpleReducer : public Barrier {
	public:
	    //////////
	    // Create a <b> SimpleReducer</i>.
	    // At each operation, a barrier wait is performed, and the
	    // operation will be performed to compute the global balue.
	    // <i>name</i> should be a static string which describes
	    // the primitive for debugging purposes.
	    SimpleReducer(const char* name);

	    //////////
	    // Destroy the SimpleReducer and free associated memory.
	    virtual ~SimpleReducer();

	    //////////
	    // Performs a global sum over all of the threads.  As soon as each
	    // thread has called sum with their local sum, each thread will
	    // return the same global sum.
	    double sum(int myrank, int numThreads, double mysum);

	    //////////
	    // Performs a global max over all of the threads.  As soon as each
	    // thread has called max with their local max, each thread will
	    // return the same global max.
	    double max(int myrank, int numThreads, double mymax);

	    //////////
	    // Performs a global min over all of the threads.  As soon as each
	    // thread has called min with their local max, each thread will
	    // return the same global max.
	    double min(int myrank, int numThreads, double mymax);

	private:
	    struct data {
		double d_d;
	    };
	    struct joinArray {
		data d_d;
		// Assumes 128 bytes in a cache line...
		char d_filler[128-sizeof(data)];
	    };
	    struct pdata {
		int d_buf;
		char d_filler[128-sizeof(int)];	
	    };
	    joinArray* d_join[2];
	    pdata* d_p;
	    int d_array_size;
	    void collectiveResize(int proc, int numThreads);

	    // Cannot copy them
	    SimpleReducer(const SimpleReducer&);
	    SimpleReducer& operator=(const SimpleReducer&);
	};
} // End namespace SCIRun

#endif



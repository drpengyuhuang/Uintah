/* REFERENCED */
static char *id="@(#) $Id$";

#include "ProcessorContext.h"
#include <SCICore/Thread/SimpleReducer.h>
#include <SCICore/Thread/Mutex.h>
#include <SCICore/Thread/Thread.h>
#include <SCICore/Exceptions/InternalError.h>
#include <iostream>

using namespace Uintah;
using namespace SCICore::Thread;
using SCICore::Exceptions::InternalError;
using namespace std;

static Mutex rootlock("ProcessorContext lock");
static ProcessorContext* rootContext = 0;

ProcessorContext*
ProcessorContext::getRootContext()
{
    if(!rootContext) {
	rootlock.lock();
	if(!rootContext){
	    rootContext = new ProcessorContext(0,0,Thread::numProcessors(),0);
	}
	rootlock.unlock();
    }
    return rootContext;
}

ProcessorContext::ProcessorContext(const ProcessorContext* parent,
				   int threadNumber, int numThreads,
				   SimpleReducer* reducer)
    : d_parent(parent), d_threadNumber(threadNumber), d_numThreads(numThreads),
      d_reducer(reducer)
{
}

ProcessorContext::~ProcessorContext()
{
}

ProcessorContext*
ProcessorContext::createContext(int threadNumber,
				int numThreads,
				SimpleReducer* reducer) const
{
    return new ProcessorContext(this, threadNumber, numThreads, reducer);
}

void
ProcessorContext::barrier_wait() const
{
    if(!d_reducer)
	throw InternalError("ProcessorContext::reducer_wait called on a ProcessorContext that has no reducer");
    d_reducer->wait(d_numThreads);
}

double
ProcessorContext::reduce_min(double mymin) const
{
    if(!d_reducer)
	throw InternalError("ProcessorContext::reducer_wait called on a ProcessorContext that has no reducer");
    return d_reducer->min(d_threadNumber, d_numThreads, mymin);
}

//
// $Log$
// Revision 1.4  2000/04/26 06:49:15  sparker
// Streamlined namespaces
//
// Revision 1.3  2000/03/17 09:30:21  sparker
// New makefile scheme: sub.mk instead of Makefile.in
// Use XML-based files for module repository
// Plus many other changes to make these two things work
//
// Revision 1.2  2000/03/16 22:08:39  dav
// Added the beginnings of cocoon docs.  Added namespaces.  Did a few other coding standards updates too
//
//


/*
 *  CrowdMonitor: Multiple reader/single writer locks
 *  $Id$
 *
 *  Written by:
 *   Author: Steve Parker
 *   Department of Computer Science
 *   University of Utah
 *   Date: June 1997
 *
 *  Copyright (C) 1997 SCI Group
 */

#include <SCICore/Thread/CrowdMonitor.h>
#include <SCICore/Thread/Guard.h>
#include <SCICore/Thread/Mutex.h>

using SCICore::Thread::Guard;

Guard::Guard(Mutex* mutex)
    : d_mutex(mutex), d_monitor(0)
{
    d_mutex->lock();
}

Guard::Guard(CrowdMonitor* crowd_monitor, Which action) 
    : d_mutex(0), d_monitor(crowd_monitor), d_action(action)
{
    if(d_action==Read)
        d_monitor->readLock();
    else
        d_monitor->writeLock();
}

Guard::~Guard()
{
    if(d_mutex)
        d_mutex->unlock();
    else if(d_action==Read)
        d_monitor->readUnlock();
    else
        d_monitor->writeUnlock();
}

//
// $Log$
// Revision 1.4  1999/08/28 03:46:47  sparker
// Final updates before integration with PSE
//
// Revision 1.3  1999/08/25 19:00:48  sparker
// More updates to bring it up to spec
// Factored out common pieces in Thread_irix and Thread_pthreads
// Factored out other "default" implementations of various primitives
//
// Revision 1.2  1999/08/25 02:37:56  sparker
// Added namespaces
// General cleanups to prepare for integration with SCIRun
//
//


/*
 *  Object.cc: Base class for all PIDL distributed objects
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   July 1999
 *
 *  Copyright (C) 1999 SCI Group
 */

#include <Core/CCA/Component/PIDL/Object.h>

#include <Core/CCA/Component/PIDL/GlobusError.h>
#include <Core/CCA/Component/PIDL/PIDL.h>
#include <Core/CCA/Component/PIDL/Reference.h>
#include <Core/CCA/Component/PIDL/ServerContext.h>
#include <Core/CCA/Component/PIDL/TypeInfo.h>
#include <Core/CCA/Component/PIDL/URL.h>
#include <Core/CCA/Component/PIDL/Wharehouse.h>
#include <Core/Exceptions/InternalError.h>
#include <Core/Thread/MutexPool.h>
#include <sstream>

using Component::PIDL::Object_interface;
using Component::PIDL::URL;

Object_interface::Object_interface()
    : d_serverContext(0)
{
    ref_cnt=0;
    mutex_index=getMutexPool()->nextIndex();
}

void Object_interface::initializeServer(const TypeInfo* typeinfo, void* ptr)
{
    if(!d_serverContext){
	d_serverContext=new ServerContext;
	d_serverContext->d_endpoint_active=false;
	d_serverContext->d_objptr=this;
    } else if(d_serverContext->d_endpoint_active){
	throw InternalError("Server re-initialized while endpoint already active?");
    } else if(d_serverContext->d_objptr != this){
	throw InternalError("Server re-initialized with a different base class ptr?");
    }
    // This may happen multiple times, due to multiple inheritance.  It
    // is a "last one wins" approach - the last CTOR to call this function
    // is the most derived type.
    d_serverContext->d_typeinfo=typeinfo;
    d_serverContext->d_ptr=ptr;
}

Object_interface::~Object_interface()
{
    if(ref_cnt != 0)
	throw InternalError("Object delete while reference count != 0");
    if(d_serverContext){
	Wharehouse* wharehouse=PIDL::getWharehouse();
	if(wharehouse->unregisterObject(d_serverContext->d_objid) != this)
	    throw InternalError("Corruption in object wharehouse");
	if(int gerr=globus_nexus_endpoint_destroy(&d_serverContext->d_endpoint))
	    throw GlobusError("endpoint_destroy", 0);
	delete d_serverContext;
    }
}

URL Object_interface::getURL() const
{
    std::ostringstream o;
    if(d_serverContext){
	if(!d_serverContext->d_endpoint_active)
	    activateObject();
	o << PIDL::getBaseURL() << d_serverContext->d_objid;
    } else {
	// TODO - send a message to get the URL
	o << "getURL() doesn't (yet) work for proxy objects";
    }
    return o.str();
}

void Object_interface::_getReference(Reference& ref, bool copy) const
{
    if(!d_serverContext)
	throw InternalError("Object_interface::getReference called for a non-server object");
    if(!copy){
	throw InternalError("Object_interface::getReference called with copy=false");
    }
    if(!d_serverContext->d_endpoint_active)
	activateObject();
    if(int gerr=globus_nexus_startpoint_bind(&ref.d_sp, &d_serverContext->d_endpoint))
	throw GlobusError("startpoint_bind", gerr);
    ref.d_vtable_base=TypeInfo::vtable_methods_start;
}

void Object_interface::_addReference()
{
    Mutex* m=getMutexPool()->getMutex(mutex_index);
    m->lock();
    ref_cnt++;
    m->unlock();
}

void Object_interface::_deleteReference()
{
    Mutex* m=getMutexPool()->getMutex(mutex_index);
    m->lock();
    ref_cnt--;
    bool del;
    if(ref_cnt == 0)
	del=true;
    else
	del=false;
    m->unlock();

    // We must delete outside of the lock to prevent deadlock
    // conditions with the mutex pool, but we must check the condition
    // inside the lock to prevent race conditions with other threads
    // simultaneously releasing a reference.
    if(del)
	delete this;
}

void Object_interface::activateObject() const
{
    Wharehouse* wharehouse=PIDL::getWharehouse();
    d_serverContext->d_objid=wharehouse->registerObject(const_cast<Object_interface*>(this));
    d_serverContext->activateEndpoint();
}

MutexPool* Object_interface::getMutexPool()
{
    static MutexPool* pool=0;
    if(!pool){
	// TODO - make this threadsafe.  This can leak if two threads
	// happen to request the first pool at the same time.  I doubt
	// it will ever happen - sparker.
	pool=new MutexPool("Core/CCA/Component::PIDL::Object_interface mutex pool", 63);
    }
    return pool;
}


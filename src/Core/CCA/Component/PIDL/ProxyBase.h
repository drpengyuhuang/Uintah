
/*
 *  ProxyBase.h: Base class for all PIDL proxies
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   July 1999
 *
 *  Copyright (C) 1999 SCI Group
 */

#ifndef Core/CCA/Component_PIDL_ProxyBase_h
#define Core/CCA/Component_PIDL_ProxyBase_h

#include <Core/CCA/Component/PIDL/Reference.h>

namespace SCIRun {

/**************************************
 
CLASS
   ProxyBase
   
KEYWORDS
   Proxy, PIDL
   
DESCRIPTION
   The base class for all proxy objects.  It contains the reference to
   the remote object.  This class should not be used outside of PIDL
   or automatically generated sidl code.
****************************************/
	class ProxyBase {
	public:
	protected:
	    ////////////
	    // Create the proxy from the given reference.
	    ProxyBase(const Reference&);

	    ///////////
	    // Destructor
	    virtual ~ProxyBase();

	    //////////
	    // The reference to the remote object.
	    Reference d_ref;

	    //////////
	    // TypeInfo is a friend so that it can call _proxyGetReference
	    friend class TypeInfo;

	    //////////
	    // Return the internal reference.  If copy is true, the startpoint
	    // will be copied through globus_nexus_startpoint_copy, and
	    // will need to be destroyed with globus_nexus_startpoint_destroy
	    // or globus_nexus_put_startpoint_transfer.
	    void _proxyGetReference(Reference&, bool copy) const;
	};
} // End namespace SCIRun

#endif


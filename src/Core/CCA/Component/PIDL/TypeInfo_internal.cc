
/*
 *  TypeInfo_internal.cc: internal representation for a type.
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   July 1999
 *
 *  Copyright (C) 1999 SCI Group
 */

#include <Core/CCA/Component/PIDL/TypeInfo_internal.h>
#include <Core/CCA/Component/PIDL/TypeInfo.h>
#include <Core/Exceptions/InternalError.h>
using Component::PIDL::TypeInfo_internal;

TypeInfo_internal::TypeInfo_internal(const std::string& fullclassname,
				     const std::string& uuid,
				     globus_nexus_handler_t* table,
				     int tableSize,
				     Object_interface* (*create_proxy)(const Reference&))
    : fullclassname(fullclassname), uuid(uuid),
      table(table), tableSize(tableSize), create_proxy(create_proxy),
      parentclass(0)
{
    // This is safe because it will not recurse - there are no parents yet.
    add_castable(this, TypeInfo::vtable_methods_start);
}

void TypeInfo_internal::add_parentclass(const TypeInfo* ti, int vtoffset)
{
    parentclass=const_cast<TypeInfo*>(ti);
    add_castable(ti->d_priv, vtoffset);
}

void TypeInfo_internal::add_parentiface(const TypeInfo* ti, int vtoffset)
{
    parent_ifaces.push_back(const_cast<TypeInfo*>(ti));
    add_castable(ti->d_priv, vtoffset);
}

void TypeInfo_internal::add_castable(const TypeInfo_internal* ti, int vtoffset)
{
    MapType::iterator iter_classname=classname_map.find(ti->fullclassname);
    if(iter_classname == classname_map.end()){
	// Insert this...
	classname_map[ti->fullclassname]=MapType::mapped_type(ti, vtoffset);
	const_cast<TypeInfo_internal*>(ti)->add_castables(this, vtoffset);
    } else {
	if(iter_classname->second.first->uuid != ti->uuid){
	    throw InternalError("inconsistent typeinfo");
	} else {
	    // Ok...
	}
    }
}

void TypeInfo_internal::add_castables(TypeInfo_internal* parent, int vtoffset)
{
    // This is an already constructed typeinfo.  We are supposed to add
    // our castable list to our parent
    for(MapType::iterator iter=classname_map.begin();
	iter != classname_map.end(); iter++){
	parent->add_castable(iter->second.first, iter->second.second+vtoffset-TypeInfo::vtable_methods_start);
    }
}


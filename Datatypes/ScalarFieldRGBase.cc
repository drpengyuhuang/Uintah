
/*
 *  ScalarFieldRGBase.cc: Scalar Fields defined on a Regular grid base class
 *
 *  Written by:
 *   Steven G. Parker (& David Weinstein)
 *   Department of Computer Science
 *   University of Utah
 *   March 1994 (& January 1996)
 *
 *  Copyright (C) 1994, 1996 SCI Group 
 */

#include <Datatypes/ScalarFieldRGBase.h>
#include <Classlib/String.h>
#include <Malloc/Allocator.h>
#include <iostream.h>

PersistentTypeID ScalarFieldRGBase::type_id("ScalarFieldRGBase", "ScalarField", 0);

ScalarFieldRGBase::ScalarFieldRGBase()
: ScalarField(RegularGrid), nx(0), ny(0), nz(0)
{
}

ScalarFieldRGBase::ScalarFieldRGBase(const ScalarFieldRGBase& copy)
: ScalarField(copy), nx(copy.nx), ny(copy.ny), nz(copy.nz)
{
}

ScalarFieldRGBase::~ScalarFieldRGBase()
{
}

Point ScalarFieldRGBase::get_point(int i, int j, int k)
{
    double x=bmin.x()+diagonal.x()*double(i)/double(nx-1);
    double y=bmin.y()+diagonal.y()*double(j)/double(ny-1);
    double z=bmin.z()+diagonal.z()*double(k)/double(nz-1);
    return Point(x,y,z);
}

void ScalarFieldRGBase::set_bounds(const Point &min, const Point &max) {
    bmin=min;
    bmax=max;
}
    
void ScalarFieldRGBase::locate(const Point& p, int& ix, int& iy, int& iz)
{
    Vector pn=p-bmin;
    double dx=diagonal.x();
    double dy=diagonal.y();
    double dz=diagonal.z();
    double x=pn.x()*(nx-1)/dx;
    double y=pn.y()*(ny-1)/dy;
    double z=pn.z()*(nz-1)/dz;
    ix=(int)x;
    iy=(int)y;
    iz=(int)z;
}

#define ScalarFieldRGBase_VERSION 1

void ScalarFieldRGBase::io(Piostream& stream)
{
    /*int version=*/stream.begin_class("ScalarFieldRGBase", ScalarFieldRGBase_VERSION);
    // Do the base class first...
    ScalarField::io(stream);

    // Save these since the ScalarField doesn't
    Pio(stream, bmin);
    Pio(stream, bmax);
    if(stream.reading()){
	have_bounds=1;
	diagonal=bmax-bmin;
    }

    // Save the rest..
    Pio(stream, nx);
    Pio(stream, ny);
    Pio(stream, nz);
    stream.end_class();
}	

void ScalarFieldRGBase::compute_bounds()
{
    // Nothing to do - we store the bounds in the base class...
}


/*
 *  Plane.cc: Uniform Plane containing triangular elements
 *
 *  Written by:
 *   David Weinstein
 *   Department of Computer Science
 *   University of Utah
 *   October 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <Geometry/Plane.h>
#include <Geometry/Point.h>
#include <Geometry/Vector.h>
#include <iostream.h>

Plane::Plane()
: n(Vector(0,0,1)), d(0)
{
}

Plane::Plane(const Plane &copy)
: n(copy.n), d(copy.d)
{
}

Plane::Plane(const Point &p1, const Point &p2, const Point &p3) {
    Vector v1(p2-p1);
    Vector v2(p2-p3);
    n=Cross(v2,v1);
    n.normalize();
    d=-Dot(p1, n);
}

Plane::~Plane() {
}

void Plane::flip() {
   n.x(-n.x());
   n.y(-n.y());
   n.z(-n.z());
}

double Plane::eval_point(const Point &p) {
    return Dot(p, n)+d;
}

Point Plane::project(const Point& p)
{
   return p-n*(d+Dot(p,n));
}

Vector Plane::normal()
{
   return n;
}

void
Plane::ChangePlane(const Point &p1, const Point &p2, const Point &p3) {
    Vector v1(p2-p1);
    Vector v2(p2-p3);
    n=Cross(v2,v1);
    n.normalize();
    d=-Dot(p1, n);
}

int
Plane::Intersect( Point s, Vector v, Point& hit )
{
  double tmp;
  Point origin( 0., 0., 0. );
  Point ptOnPlane = origin - n * d;

  tmp = Dot( n, v );

  if( tmp > -1.e-6 && tmp < 1.e-6 ) // Vector v is parallel to plane
    {
      // vector from origin of line to point on plane
      
      Vector temp = s - ptOnPlane;

      if ( temp.length() < 1.e-5 )
	{
	  // the origin of plane and the origin of line are almost
	  // the same
	  
	  hit = ptOnPlane;
	  return 1;
	}
      else
	{
	  // point s and d are not the same, but maybe s lies
	  // in the plane anyways
	  
	  tmp = Dot( temp, n );

	  if(tmp > -1.e-6 && tmp < 1.e-6)
	    {
	      hit = s;
	      return 1;
	    }
	  else
	    return 0;
	}
    }

  tmp = - ( ( d + Dot( s, n ) ) / Dot( v, n ) );

#if 0  
  // the starting point s virtually lies in the plane already
  
  if ( tmp > -1.e-6 && tmp < 1.e-6 )
    hit = s;
  else
    hit = s + v * tmp;
#endif

  hit = s + v * tmp;

  return 1;
}

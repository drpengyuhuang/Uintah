
/*
 *  RingWidget.h
 *
 *  Written by:
 *   James Purciful
 *   Department of Computer Science
 *   University of Utah
 *   Jan. 1995
 *
 *  Copyright (C) 1995 SCI Group
 */


#include <Widgets/RingWidget.h>
#include <Constraints/AngleConstraint.h>
#include <Constraints/DistanceConstraint.h>
#include <Constraints/MidpointConstraint.h>
#include <Constraints/PlaneConstraint.h>
#include <Constraints/RatioConstraint.h>
#include <Geom/Cylinder.h>
#include <Geom/Sphere.h>
#include <Geom/Torus.h>
#include <Geometry/Plane.h>

const Index NumCons = 13;
const Index NumVars = 11;
const Index NumGeoms = 7;
const Index NumMatls = 5;
const Index NumPcks = 7;
const Index NumSchemes = 4;

enum { RingW_ConstULDR, RingW_ConstURDL, RingW_ConstHypo, RingW_ConstPlane,
       RingW_ConstULUR, RingW_ConstULDL, RingW_ConstDRUR, RingW_ConstDRDL,
       RingW_ConstSDist, RingW_ConstCDist, RingW_ConstMidpoint, RingW_ConstSPlane,
       RingW_ConstAngle };
enum { RingW_GeomPointUL, RingW_GeomPointUR, RingW_GeomPointDR, RingW_GeomPointDL,
       RingW_GeomSlider, RingW_GeomRing, RingW_GeomResize };
enum { RingW_PickSphUL, RingW_PickSphUR, RingW_PickSphDR, RingW_PickSphDL, RingW_PickCyls,
       RingW_PickSlider, RingW_PickResize };

RingWidget::RingWidget( Module* module, CrowdMonitor* lock, Real widget_scale )
: BaseWidget(module, lock, NumVars, NumCons, NumGeoms, NumMatls, NumPcks, widget_scale*0.1),
  oldaxis1(1, 0, 0), oldaxis2(0, 1, 0)
{
   Real INIT = 1.0*widget_scale;
   // Scheme4 is used to resize.
   variables[RingW_PointUL] = new PointVariable("PntUL", Scheme1, Point(0, 0, 0));
   variables[RingW_PointUR] = new PointVariable("PntUR", Scheme2, Point(INIT, 0, 0));
   variables[RingW_PointDR] = new PointVariable("PntDR", Scheme1, Point(INIT, INIT, 0));
   variables[RingW_PointDL] = new PointVariable("PntDL", Scheme2, Point(0, INIT, 0));
   variables[RingW_Center] = new PointVariable("Center", Scheme2, Point(INIT/2.0, INIT/2.0, 0));
   variables[RingW_Slider] = new PointVariable("Slider", Scheme3, Point(0, 0, 0));
   variables[RingW_Dist] = new RealVariable("Dist", Scheme1, INIT);
   variables[RingW_Hypo] = new RealVariable("Hypo", Scheme1, sqrt(2*INIT*INIT));
   variables[RingW_Const] = new RealVariable("sqrt(2)", Scheme1, sqrt(2));
   variables[RingW_SDist] = new RealVariable("SDist", Scheme3, sqrt(INIT*INIT/2.0));
   variables[RingW_Angle] = new RealVariable("Angle", Scheme1, 0);

   constraints[RingW_ConstAngle] = new AngleConstraint("ConstAngle",
						       NumSchemes,
						       variables[RingW_Center],
						       variables[RingW_PointUL],
						       variables[RingW_PointUR],
						       variables[RingW_Slider],
						       variables[RingW_Angle]);
   constraints[RingW_ConstAngle]->VarChoices(Scheme1, 3, 3, 3, 3, 3);
   constraints[RingW_ConstAngle]->VarChoices(Scheme2, 3, 3, 3, 3, 3);
   constraints[RingW_ConstAngle]->VarChoices(Scheme3, 4, 4, 4, 4, 4);
   constraints[RingW_ConstAngle]->VarChoices(Scheme4, 3, 3, 3, 3, 3);
   constraints[RingW_ConstAngle]->Priorities(P_Default, P_Default, P_Default,
					     P_Default, P_Highest);
   constraints[RingW_ConstSDist] = new DistanceConstraint("ConstSDist",
							  NumSchemes,
							  variables[RingW_Center],
							  variables[RingW_Slider],
							  variables[RingW_SDist]);
   constraints[RingW_ConstSDist]->VarChoices(Scheme1, 1, 1, 1);
   constraints[RingW_ConstSDist]->VarChoices(Scheme2, 1, 1, 1);
   constraints[RingW_ConstSDist]->VarChoices(Scheme3, 1, 1, 1);
   constraints[RingW_ConstSDist]->VarChoices(Scheme4, 1, 1, 1);
   constraints[RingW_ConstSDist]->Priorities(P_Lowest, P_HighMedium, P_Default);
   constraints[RingW_ConstCDist] = new DistanceConstraint("ConstCDist",
							  NumSchemes,
							  variables[RingW_PointDL],
							  variables[RingW_Center],
							  variables[RingW_SDist]);
   constraints[RingW_ConstCDist]->VarChoices(Scheme1, 2, 2, 2);
   constraints[RingW_ConstCDist]->VarChoices(Scheme2, 2, 2, 2);
   constraints[RingW_ConstCDist]->VarChoices(Scheme3, 2, 2, 2);
   constraints[RingW_ConstCDist]->VarChoices(Scheme4, 2, 2, 2);
   constraints[RingW_ConstCDist]->Priorities(P_Default, P_Default, P_Highest);
   constraints[RingW_ConstMidpoint] = new MidpointConstraint("ConstMidpoint",
							     NumSchemes,
							     variables[RingW_PointUL],
							     variables[RingW_PointDR],
							     variables[RingW_Center]);
   constraints[RingW_ConstMidpoint]->VarChoices(Scheme1, 2, 2, 2);
   constraints[RingW_ConstMidpoint]->VarChoices(Scheme2, 2, 2, 2);
   constraints[RingW_ConstMidpoint]->VarChoices(Scheme3, 2, 2, 2);
   constraints[RingW_ConstMidpoint]->VarChoices(Scheme4, 2, 2, 2);
   constraints[RingW_ConstMidpoint]->Priorities(P_Default, P_Default, P_Highest);
   constraints[RingW_ConstPlane] = new PlaneConstraint("ConstPlane",
						       NumSchemes,
						       variables[RingW_PointUL],
						       variables[RingW_PointUR],
						       variables[RingW_PointDR],
						       variables[RingW_PointDL]);
   constraints[RingW_ConstPlane]->VarChoices(Scheme1, 2, 3, 0, 1);
   constraints[RingW_ConstPlane]->VarChoices(Scheme2, 2, 3, 0, 1);
   constraints[RingW_ConstPlane]->VarChoices(Scheme3, 2, 3, 0, 1);
   constraints[RingW_ConstPlane]->VarChoices(Scheme4, 2, 3, 0, 1);
   constraints[RingW_ConstPlane]->Priorities(P_Highest, P_Highest,
					     P_Highest, P_Highest);
   constraints[RingW_ConstSPlane] = new PlaneConstraint("ConstSPlane",
							NumSchemes,
							variables[RingW_PointDL],
							variables[RingW_PointUR],
							variables[RingW_PointDR],
							variables[RingW_Slider]);
   constraints[RingW_ConstSPlane]->VarChoices(Scheme1, 3, 3, 3, 3);
   constraints[RingW_ConstSPlane]->VarChoices(Scheme2, 3, 3, 3, 3);
   constraints[RingW_ConstSPlane]->VarChoices(Scheme3, 3, 3, 3, 3);
   constraints[RingW_ConstSPlane]->VarChoices(Scheme4, 3, 3, 3, 3);
   constraints[RingW_ConstSPlane]->Priorities(P_Highest, P_Highest,
					      P_Highest, P_Highest);
   constraints[RingW_ConstULDR] = new DistanceConstraint("Const13",
							 NumSchemes,
							 variables[RingW_PointUL],
							 variables[RingW_PointDR],
							 variables[RingW_Hypo]);
   constraints[RingW_ConstULDR]->VarChoices(Scheme1, 1, 0, 1);
   constraints[RingW_ConstULDR]->VarChoices(Scheme2, 1, 0, 1);
   constraints[RingW_ConstULDR]->VarChoices(Scheme3, 2, 2, 1);
   constraints[RingW_ConstULDR]->VarChoices(Scheme4, 2, 2, 1);
   constraints[RingW_ConstULDR]->Priorities(P_HighMedium, P_HighMedium, P_Default);
   constraints[RingW_ConstURDL] = new DistanceConstraint("Const24",
							 NumSchemes,
							 variables[RingW_PointUR],
							 variables[RingW_PointDL],
							 variables[RingW_Hypo]);
   constraints[RingW_ConstURDL]->VarChoices(Scheme1, 1, 0, 1);
   constraints[RingW_ConstURDL]->VarChoices(Scheme2, 1, 0, 1);
   constraints[RingW_ConstURDL]->VarChoices(Scheme3, 1, 0, 1);
   constraints[RingW_ConstURDL]->VarChoices(Scheme4, 1, 0, 1);
   constraints[RingW_ConstURDL]->Priorities(P_HighMedium, P_HighMedium, P_Default);
   constraints[RingW_ConstHypo] = new RatioConstraint("ConstRatio",
						      NumSchemes,
						      variables[RingW_Hypo],
						      variables[RingW_Dist],
						      variables[RingW_Const]);
   constraints[RingW_ConstHypo]->VarChoices(Scheme1, 1, 0, 1);
   constraints[RingW_ConstHypo]->VarChoices(Scheme2, 1, 0, 1);
   constraints[RingW_ConstHypo]->VarChoices(Scheme3, 1, 0, 1);
   constraints[RingW_ConstHypo]->VarChoices(Scheme4, 1, 0, 1);
   constraints[RingW_ConstHypo]->Priorities(P_HighMedium, P_Default, P_Lowest);
   constraints[RingW_ConstULUR] = new DistanceConstraint("Const12",
							 NumSchemes,
							 variables[RingW_PointUL],
							 variables[RingW_PointUR],
							 variables[RingW_Dist]);
   constraints[RingW_ConstULUR]->VarChoices(Scheme1, 1, 1, 1);
   constraints[RingW_ConstULUR]->VarChoices(Scheme2, 0, 0, 0);
   constraints[RingW_ConstULUR]->VarChoices(Scheme3, 1, 1, 1);
   constraints[RingW_ConstULUR]->VarChoices(Scheme4, 1, 1, 1);
   constraints[RingW_ConstULUR]->Priorities(P_Default, P_Default, P_LowMedium);
   constraints[RingW_ConstULDL] = new DistanceConstraint("Const14",
							 NumSchemes,
							 variables[RingW_PointUL],
							 variables[RingW_PointDL],
							 variables[RingW_Dist]);
   constraints[RingW_ConstULDL]->VarChoices(Scheme1, 1, 1, 1);
   constraints[RingW_ConstULDL]->VarChoices(Scheme2, 0, 0, 0);
   constraints[RingW_ConstULDL]->VarChoices(Scheme3, 1, 1, 1);
   constraints[RingW_ConstULDL]->VarChoices(Scheme4, 1, 1, 1);
   constraints[RingW_ConstULDL]->Priorities(P_Default, P_Default, P_LowMedium);
   constraints[RingW_ConstDRUR] = new DistanceConstraint("Const32",
							 NumSchemes,
							 variables[RingW_PointDR],
							 variables[RingW_PointUR],
							 variables[RingW_Dist]);
   constraints[RingW_ConstDRUR]->VarChoices(Scheme1, 1, 1, 1);
   constraints[RingW_ConstDRUR]->VarChoices(Scheme2, 0, 0, 0);
   constraints[RingW_ConstDRUR]->VarChoices(Scheme3, 1, 1, 1);
   constraints[RingW_ConstDRUR]->VarChoices(Scheme4, 1, 1, 1);
   constraints[RingW_ConstDRUR]->Priorities(P_Default, P_Default, P_LowMedium);
   constraints[RingW_ConstDRDL] = new DistanceConstraint("Const34",
							 NumSchemes,
							 variables[RingW_PointDR],
							 variables[RingW_PointDL],
							 variables[RingW_Dist]);
   constraints[RingW_ConstDRDL]->VarChoices(Scheme1, 1, 1, 1);
   constraints[RingW_ConstDRDL]->VarChoices(Scheme2, 0, 0, 0);
   constraints[RingW_ConstDRDL]->VarChoices(Scheme3, 1, 1, 1);
   constraints[RingW_ConstDRDL]->VarChoices(Scheme4, 1, 1, 1);
   constraints[RingW_ConstDRDL]->Priorities(P_Default, P_Default, P_LowMedium);

   materials[RingW_PointMatl] = PointWidgetMaterial;
   materials[RingW_EdgeMatl] = EdgeWidgetMaterial;
   materials[RingW_SliderMatl] = SliderWidgetMaterial;
   materials[RingW_SpecialMatl] = SpecialWidgetMaterial;
   materials[RingW_HighMatl] = HighlightWidgetMaterial;

   Index geom, pick;
   GeomGroup* pts = new GeomGroup;
   for (geom = RingW_GeomPointUL, pick = RingW_PickSphUL;
	geom <= RingW_GeomPointDL; geom++, pick++) {
      geometries[geom] = new GeomSphere;
      picks[pick] = new GeomPick(geometries[geom], module);
      picks[pick]->set_highlight(materials[RingW_HighMatl]);
      picks[pick]->set_cbdata((void*)pick);
      pts->add(picks[pick]);
   }
   GeomMaterial* ptsm = new GeomMaterial(pts, materials[RingW_PointMatl]);
   
   geometries[RingW_GeomRing] = new GeomTorus;
   picks[RingW_PickCyls] = new GeomPick(geometries[RingW_GeomRing], module);
   picks[RingW_PickCyls]->set_highlight(materials[RingW_HighMatl]);
   picks[RingW_PickCyls]->set_cbdata((void*)RingW_PickCyls);
   GeomMaterial* cylsm = new GeomMaterial(picks[RingW_PickCyls], materials[RingW_EdgeMatl]);

   geometries[RingW_GeomResize] = new GeomCappedCylinder;
   picks[RingW_PickResize] = new GeomPick(geometries[RingW_GeomResize], module);
   picks[RingW_PickResize]->set_highlight(materials[RingW_HighMatl]);
   picks[RingW_PickResize]->set_cbdata((void*)RingW_PickResize);
   GeomMaterial* resizem = new GeomMaterial(picks[RingW_PickResize], materials[RingW_SpecialMatl]);

   geometries[RingW_GeomSlider] = new GeomCappedCylinder;
   picks[RingW_PickSlider] = new GeomPick(geometries[RingW_GeomSlider], module);
   picks[RingW_PickSlider]->set_highlight(materials[RingW_HighMatl]);
   picks[RingW_PickSlider]->set_cbdata((void*)RingW_PickSlider);
   GeomMaterial* slidersm = new GeomMaterial(picks[RingW_PickSlider], materials[RingW_SliderMatl]);

   GeomGroup* w = new GeomGroup;
   w->add(ptsm);
   w->add(cylsm);
   w->add(resizem);
   w->add(slidersm);

   SetEpsilon(widget_scale*1e-6);
   
   FinishWidget(w);
}


RingWidget::~RingWidget()
{
}


void
RingWidget::widget_execute()
{
   ((GeomSphere*)geometries[RingW_GeomPointUL])->move(variables[RingW_PointUL]->GetPoint(),
						      1*widget_scale);
   ((GeomSphere*)geometries[RingW_GeomPointUR])->move(variables[RingW_PointUR]->GetPoint(),
						      1*widget_scale);
   ((GeomSphere*)geometries[RingW_GeomPointDR])->move(variables[RingW_PointDR]->GetPoint(),
						      1*widget_scale);
   ((GeomSphere*)geometries[RingW_GeomPointDL])->move(variables[RingW_PointDL]->GetPoint(),
						      1*widget_scale);
   Vector normal(Plane(variables[RingW_PointUL]->GetPoint(),
		       variables[RingW_PointUR]->GetPoint(),
		       variables[RingW_PointDL]->GetPoint()).normal());
   Real rad = (variables[RingW_PointUL]->GetPoint()-variables[RingW_PointDR]->GetPoint()).length()/2.;
   ((GeomTorus*)geometries[RingW_GeomRing])->move(variables[RingW_Center]->GetPoint(), normal,
						  rad, 0.5*widget_scale);
   Vector v = variables[RingW_Slider]->GetPoint()-variables[RingW_Center]->GetPoint();
   Vector slide;
   if (v.length2() > 1e-6)
      slide = Cross(normal, v.normal());
   else
      slide = Vector(0,0,0);
   ((GeomCappedCylinder*)geometries[RingW_GeomSlider])->move(variables[RingW_Slider]->GetPoint()
							     - (slide * 0.3 * widget_scale),
							     variables[RingW_Slider]->GetPoint()
							     + (slide * 0.3 * widget_scale),
							     1.1*widget_scale);
   v = variables[RingW_PointDR]->GetPoint()-variables[RingW_Center]->GetPoint();
   Vector resize;
   if (v.length2() > 1e-6)
      resize = v.normal();
   else
      resize = Vector(0,0,0);
   ((GeomCappedCylinder*)geometries[RingW_GeomResize])->move(variables[RingW_PointDR]->GetPoint(),
							     variables[RingW_PointDR]->GetPoint()
							     + (resize * 1.5 * widget_scale),
							     0.5*widget_scale);
   
   ((DistanceConstraint*)constraints[RingW_ConstULUR])->SetMinimum(3.2*widget_scale);
   ((DistanceConstraint*)constraints[RingW_ConstDRDL])->SetMinimum(3.2*widget_scale);
   ((DistanceConstraint*)constraints[RingW_ConstULDL])->SetMinimum(3.2*widget_scale);
   ((DistanceConstraint*)constraints[RingW_ConstDRUR])->SetMinimum(3.2*widget_scale);
   ((DistanceConstraint*)constraints[RingW_ConstULDR])->SetMinimum(sqrt(2*3.2*3.2)*widget_scale);
   ((DistanceConstraint*)constraints[RingW_ConstURDL])->SetMinimum(sqrt(2*3.2*3.2)*widget_scale);

   SetEpsilon(widget_scale*1e-6);

   Vector spvec1(variables[RingW_PointUR]->GetPoint() - variables[RingW_PointUL]->GetPoint());
   Vector spvec2(variables[RingW_PointDL]->GetPoint() - variables[RingW_PointUL]->GetPoint());
   if ((spvec1.length2() > 0.0) && (spvec2.length2() > 0.0)) {
      spvec1.normalize();
      spvec2.normalize();
      Vector v = Cross(spvec1, spvec2);
      for (Index geom = 0; geom < NumPcks; geom++) {
	 if (geom == RingW_PickSlider)
	    picks[geom]->set_principal(slide);
	 else if (geom == RingW_PickResize)
	    picks[geom]->set_principal(resize);
	 else
	    picks[geom]->set_principal(spvec1, spvec2, v);
      }
   }
}


void
RingWidget::geom_moved( int /* axis */, double /* dist */, const Vector& delta,
			void* cbdata )
{
   ((DistanceConstraint*)constraints[RingW_ConstULUR])->SetDefault(GetAxis1());
   ((DistanceConstraint*)constraints[RingW_ConstDRDL])->SetDefault(GetAxis1());
   ((DistanceConstraint*)constraints[RingW_ConstULDL])->SetDefault(GetAxis2());
   ((DistanceConstraint*)constraints[RingW_ConstDRUR])->SetDefault(GetAxis2());

   for (Index v=0; v<NumVars; v++)
      variables[v]->Reset();
   
   switch((int)cbdata){
   case RingW_PickSphUL:
      variables[RingW_PointUL]->SetDelta(delta);
      break;
   case RingW_PickSphUR:
      variables[RingW_PointUR]->SetDelta(delta);
      break;
   case RingW_PickSphDR:
      variables[RingW_PointDR]->SetDelta(delta);
      break;
   case RingW_PickSphDL:
      variables[RingW_PointDL]->SetDelta(delta);
      break;
   case RingW_PickResize:
      variables[RingW_PointUL]->MoveDelta(-delta);
      variables[RingW_PointDR]->SetDelta(delta, Scheme4);
      break;
   case RingW_PickSlider:
      variables[RingW_Slider]->SetDelta(delta);
      break;
   case RingW_PickCyls:
      variables[RingW_PointUL]->MoveDelta(delta);
      variables[RingW_PointUR]->MoveDelta(delta);
      variables[RingW_PointDR]->MoveDelta(delta);
      variables[RingW_PointDL]->MoveDelta(delta);
      variables[RingW_Center]->MoveDelta(delta);
      variables[RingW_Slider]->MoveDelta(delta);
      break;
   }
}


void
RingWidget::SetPosition( const Point& center, const Vector& normal, const Real radius )
{
   NOT_FINISHED("SetPosition");
   Vector v1, v2;
   normal.find_orthogonal(v1, v2);
   variables[RingW_Center]->Move(center);
   variables[RingW_PointUL]->Move(center+v1*radius);
   variables[RingW_PointUR]->Move(center+v2*radius);
   variables[RingW_PointDR]->Move(center-v1*radius);
   variables[RingW_PointDL]->Set(center-v2*radius);
   execute();
}


void
RingWidget::GetPosition( Point& center, Vector& normal, Real& radius ) const
{
   center = variables[RingW_Center]->GetPoint();
   normal = Plane(variables[RingW_PointUL]->GetPoint(),
		  variables[RingW_PointUR]->GetPoint(),
		  variables[RingW_PointDL]->GetPoint()).normal();
   radius = variables[RingW_SDist]->GetReal();
}


void
RingWidget::SetRatio( const Real ratio )
{
   ASSERT((ratio>=0.0) && (ratio<=1.0));
   variables[RingW_Angle]->Set(ratio);
   execute();
}


Real
RingWidget::GetRatio() const
{
   return (variables[RingW_Angle]->GetReal() + 3.14159) / (2.0 * 3.14159);
}


void
RingWidget::SetRadius( const Real radius )
{
   ASSERT(radius>=0.0);
   
   Vector axis1(variables[RingW_PointUL]->GetPoint() - variables[RingW_Center]->GetPoint());
   Vector axis2(variables[RingW_PointUR]->GetPoint() - variables[RingW_Center]->GetPoint());
   Real ratio(radius/variables[RingW_SDist]->GetReal());

   variables[RingW_PointUL]->Move(variables[RingW_Center]->GetPoint()+axis1*ratio);
   variables[RingW_PointDR]->Move(variables[RingW_Center]->GetPoint()-axis1*ratio);
   variables[RingW_PointUR]->Move(variables[RingW_Center]->GetPoint()+axis2*ratio);
   variables[RingW_PointDL]->Move(variables[RingW_Center]->GetPoint()-axis2*ratio);

   variables[RingW_Dist]->Move(variables[RingW_Dist]->GetReal()*ratio);
   variables[RingW_Hypo]->Move(variables[RingW_Hypo]->GetReal()*ratio);
   
   variables[RingW_SDist]->Set(radius); // This should set the slider...

   execute();
}

Real
RingWidget::GetRadius() const
{
   return variables[RingW_SDist]->GetReal();
}

   
const Vector&
RingWidget::GetAxis1()
{
   Vector axis(variables[RingW_PointUR]->GetPoint() - variables[RingW_PointUL]->GetPoint());
   if (axis.length2() <= 1e-6)
      return oldaxis1;
   else
      return (oldaxis1 = axis.normal());
}


const Vector&
RingWidget::GetAxis2()
{
   Vector axis(variables[RingW_PointDL]->GetPoint() - variables[RingW_PointUL]->GetPoint());
   if (axis.length2() <= 1e-6)
      return oldaxis2;
   else
      return (oldaxis2 = axis.normal());
}



#ifndef UINTAH_GRID_BoundCondData_H
#define UINTAH_GRID_BoundCondData_H

#include <vector>
#include <map>
#include <string>
using std::vector;
using std::map;
using std::string;

namespace Uintah {

  class BoundCondBase;
/**************************************

CLASS
   BoundCondData
   
   
GENERAL INFORMATION

   BoundCondData.h

   John A. Schmidt
   Department of Mechanical Engineering
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2001 SCI Group

KEYWORDS
   BoundCondBase

DESCRIPTION
   Long description...
  
WARNING
  
****************************************/

  class BCData  {
  public:
    BCData();
    ~BCData();
    void setBCValues(int mat_id,BoundCondBase* bc);
    BoundCondBase* getBCValues(int mat_id,const string& type) const;
    
   private:
    // The vector is for the material id, the map is for the name of the
    // bc type and then the actual bc data, i.e. 
    // "Velocity", VelocityBoundCond
    vector<map<string,BoundCondBase*> > d_data;
    
  };
} // End namespace Uintah

#endif



#ifndef UINTAH_HOMEBREW_CFDINTERFACE_H
#define UINTAH_HOMEBREW_CFDINTERFACE_H

#include <Packages/Uintah/Parallel/Packages/UintahParallelPort.h>
#include <Packages/Uintah/Interface/DataWarehouseP.h>
#include <Packages/Uintah/Grid/GridP.h>
#include <Packages/Uintah/Grid/LevelP.h>
#include <Packages/Uintah/Grid/SimulationStateP.h>
#include <Packages/Uintah/Interface/SchedulerP.h>
#include <Packages/Uintah/Interface/ProblemSpecP.h>

namespace Uintah {
      /**************************************
	
	CLASS
          CFDInterface
	
	  Short description...
	
	GENERAL INFORMATION
	
          CFDInterface.h
	
	  Steven G. Parker
	  Department of Computer Science
	  University of Utah
	
	  Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
	
	  Copyright (C) 2000 SCI Group
	
	KEYWORDS
          CFD_Interface
	
	DESCRIPTION
          Long description...
	
	WARNING
	
	****************************************/
      
   class CFDInterface : public Packages/UintahParallelPort {
   public:
      CFDInterface();
      virtual ~CFDInterface();
      
      //////////
      // Insert Documentation Here:
      virtual void problemSetup(const ProblemSpecP& params, GridP& grid,
				SimulationStateP& state) = 0;
      
      //////////
      // Insert Documentation Here:
      virtual void scheduleComputeStableTimestep(const LevelP& level,
						 SchedulerP&,
						 DataWarehouseP&) = 0;
      
      //////////
      // Insert Documentation Here:
      virtual void scheduleInitialize(const LevelP& level,
				      SchedulerP&,
				      DataWarehouseP&) = 0;
      
      //////////
      // Insert Documentation Here:
      virtual void scheduleTimeAdvance(double t, double dt,
				       const LevelP& level, SchedulerP&,
				       DataWarehouseP&, DataWarehouseP&) = 0;

   private:
      CFDInterface(const CFDInterface&);
      CFDInterface& operator=(const CFDInterface&);
   };
} // End namespace Uintah
   


#endif


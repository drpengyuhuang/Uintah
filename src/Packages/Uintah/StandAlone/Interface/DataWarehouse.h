#ifndef UINTAH_HOMEBREW_DataWarehouse_H
#define UINTAH_HOMEBREW_DataWarehouse_H


#include <Packages/Uintah/Grid/Handle.h>
#include <Packages/Uintah/Grid/GridP.h>
#include <Packages/Uintah/Grid/CCVariableBase.h>
#include <Packages/Uintah/Grid/Ghost.h>
#include <Packages/Uintah/Grid/RefCounted.h>
#include <Packages/Uintah/Grid/ParticleVariableBase.h>
#include <Packages/Uintah/Grid/NCVariableBase.h>
#include <Packages/Uintah/Grid/XFCVariableBase.h>
#include <Packages/Uintah/Grid/YFCVariableBase.h>
#include <Packages/Uintah/Grid/ZFCVariableBase.h>
#include <Packages/Uintah/Grid/SFCXVariableBase.h>
#include <Packages/Uintah/Grid/SFCYVariableBase.h>
#include <Packages/Uintah/Grid/SFCZVariableBase.h>
#include <Packages/Uintah/Grid/ReductionVariableBase.h>
#include <Packages/Uintah/Grid/PerPatchBase.h>
#include <Packages/Uintah/Interface/DataWarehouseP.h>
#include <Packages/Uintah/Interface/SchedulerP.h>

#include <iosfwd>

using namespace std;
namespace SCIRun {
  class Vector;
}

namespace Uintah {
/**************************************
	
CLASS
   DataWarehouse
	
   Short description...
	
GENERAL INFORMATION
	
   DataWarehouse.h
	
   Steven G. Parker
   Department of Computer Science
   University of Utah
	
   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
	
   Copyright (C) 2000 SCI Group
	
KEYWORDS
   DataWarehouse
	
DESCRIPTION
   Long description...
	
WARNING
	
****************************************/
      
   class DataWarehouse : public RefCounted {

   public:
      virtual ~DataWarehouse();
      
      DataWarehouseP getTop() const;
      
      virtual void setGrid(const GridP&)=0;

      virtual bool exists(const VarLabel*, int matlIndex, const Patch*) const =0;
      
      // Reduction Variables
      virtual void allocate(ReductionVariableBase&, const VarLabel*,
			    int matlIndex = -1) = 0;
      virtual void get(ReductionVariableBase&, const VarLabel*,
		       int matlIndex = -1) = 0;
      virtual void put(const ReductionVariableBase&, const VarLabel*,
		       int matlIndex = -1) = 0;
      virtual void override(const ReductionVariableBase&, const VarLabel*,
			    int matlIndex = -1) = 0;

      // Scatther/gather.  This will need a VarLabel if anyone but the
      // scheduler ever wants to use it.
      virtual void scatter(ScatterGatherBase*, const Patch*, const Patch*) = 0;
      virtual ScatterGatherBase* gather(const Patch*, const Patch*) = 0;
      
      // Particle Variables
      virtual ParticleSubset* createParticleSubset(particleIndex numParticles,
				        int matlIndex, const Patch*) = 0;
      virtual bool haveParticleSubset(int matlIndex, const Patch*) = 0;
      virtual ParticleSubset* getParticleSubset(int matlIndex,
					const Patch*) = 0;
      virtual ParticleSubset* getParticleSubset(int matlIndex,
			 const Patch*, Ghost::GhostType, int numGhostCells,
			 const VarLabel* posvar) = 0;
      virtual void allocate(ParticleVariableBase&, const VarLabel*,
			    ParticleSubset*) = 0;
      virtual void get(ParticleVariableBase&, const VarLabel*,
		       ParticleSubset*) = 0;
      virtual void put(const ParticleVariableBase&, const VarLabel*) = 0;
      virtual ParticleVariableBase* getParticleVariable(const VarLabel*,
							ParticleSubset*) = 0;
      
      // Node Centered (NC) Variables
      virtual void allocate(NCVariableBase&, const VarLabel*,
			    int matlIndex, const Patch*) = 0;
      virtual void get(NCVariableBase&, const VarLabel*, int matlIndex,
		       const Patch*, Ghost::GhostType, int numGhostCells) = 0;
      virtual void put(const NCVariableBase&, const VarLabel*,
		       int matlIndex, const Patch*) = 0;
      
      // Cell Centered (CC) Variables
      virtual void allocate(CCVariableBase&, const VarLabel*,
			    int matlIndex, const Patch*) = 0;
      virtual void get(CCVariableBase&, const VarLabel*, int matlIndex,
		       const Patch*, Ghost::GhostType, int numGhostCells) = 0;
      virtual void put(const CCVariableBase&, const VarLabel*,
		       int matlIndex, const Patch*) = 0;

      // Face  Centered (XFC) Variables
      virtual void allocate(XFCVariableBase&, const VarLabel*,
			    int matlIndex, const Patch*) = 0;
      virtual void get(XFCVariableBase&, const VarLabel*, int matlIndex,
		       const Patch*, Ghost::GhostType, int numGhostCells) = 0;
      virtual void put(const XFCVariableBase&, const VarLabel*,
		       int matlIndex, const Patch*) = 0;

      // Face  Centered (YFC) Variables
      virtual void allocate(YFCVariableBase&, const VarLabel*,
			    int matlIndex, const Patch*) = 0;
      virtual void get(YFCVariableBase&, const VarLabel*, int matlIndex,
		       const Patch*, Ghost::GhostType, int numGhostCells) = 0;
      virtual void put(const YFCVariableBase&, const VarLabel*,
		       int matlIndex, const Patch*) = 0;


      // Face  Centered (ZFC) Variables
      virtual void allocate(ZFCVariableBase&, const VarLabel*,
			    int matlIndex, const Patch*) = 0;
      virtual void get(ZFCVariableBase&, const VarLabel*, int matlIndex,
		       const Patch*, Ghost::GhostType, int numGhostCells) = 0;
      virtual void put(const ZFCVariableBase&, const VarLabel*,
		       int matlIndex, const Patch*) = 0;

      // Staggered Variables in all three directions (SFCX, SFCY, SFCZ)
      virtual void allocate(SFCXVariableBase&, const VarLabel*,
			    int matlIndex, const Patch*) = 0;
      virtual void get(SFCXVariableBase&, const VarLabel*, int matlIndex,
		       const Patch*, Ghost::GhostType, int numGhostCells) = 0;
      virtual void put(const SFCXVariableBase&, const VarLabel*,
		       int matlIndex, const Patch*) = 0;

      virtual void allocate(SFCYVariableBase&, const VarLabel*,
			    int matlIndex, const Patch*) = 0;
      virtual void get(SFCYVariableBase&, const VarLabel*, int matlIndex,
		       const Patch*, Ghost::GhostType, int numGhostCells) = 0;
      virtual void put(const SFCYVariableBase&, const VarLabel*,
		       int matlIndex, const Patch*) = 0;

      virtual void allocate(SFCZVariableBase&, const VarLabel*,
			    int matlIndex, const Patch*) = 0;
      virtual void get(SFCZVariableBase&, const VarLabel*, int matlIndex,
		       const Patch*, Ghost::GhostType, int numGhostCells) = 0;
      virtual void put(const SFCZVariableBase&, const VarLabel*,
		       int matlIndex, const Patch*) = 0;

      // PerPatch Variables
      virtual void get(PerPatchBase&, const VarLabel*,
				int matlIndex, const Patch*) = 0;
      virtual void put(const PerPatchBase&, const VarLabel*,
				int matlIndex, const Patch*) = 0;
     
      // Remove particles that are no longer relevant
      virtual void deleteParticles(ParticleSubset* delset) = 0;


      virtual void emit(OutputContext&, const VarLabel* label,
			int matlIndex, const Patch* patch) const = 0;

      virtual void emit(ostream& intout, const VarLabel* label,
			int matlIndex = -1) const = 0;

      // For the schedulers
      virtual bool isFinalized() const = 0;
      virtual bool exists(const VarLabel*, const Patch*) const = 0;
      virtual void finalize() = 0;

      int getID() const {
	 return d_generation;
      }
   protected:
      DataWarehouse(const ProcessorGroup* myworld, int generation, DataWarehouseP& parent_dw);
      // These two things should be removed from here if possible - Steve
      const ProcessorGroup* d_myworld;
      const int d_generation;

      
   private:
      
      DataWarehouse(const DataWarehouse&);
      DataWarehouse& operator=(const DataWarehouse&);
      DataWarehouseP& d_parent;
   };
} // End namespace Uintah



#endif

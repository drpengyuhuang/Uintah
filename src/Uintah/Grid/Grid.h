#ifndef UINTAH_HOMEBREW_GRID_H
#define UINTAH_HOMEBREW_GRID_H

#include "GridP.h"
#include "Handle.h"
#include "LevelP.h"
#include "RefCounted.h"
#include <vector>

namespace Uintah {

/**************************************

CLASS
   Grid
   
   This class manages the grid used to solve the CFD and MPM problems.

GENERAL INFORMATION

   Grid.h

   Steven G. Parker
   Department of Computer Science
   University of Utah

   Center for the Simulation of Accidental Fires and Explosions (C-SAFE)
  
   Copyright (C) 2000 SCI Group

KEYWORDS
   Grid, Level

DESCRIPTION
   This class basicly manages the pointers to the levels that make
   up the grid.  
  
WARNING
  
****************************************/

   class Grid : public RefCounted {
   public:
      Grid();
      virtual ~Grid();
      
      //////////
      // Returns the number of levels in this grid.
      int     numLevels() const;
      
      //////////
      // Returns a "Handle" to the "idx"th level 
      const LevelP& getLevel(int idx) const;
      
      //////////
      // Adds a level to the grid.
      void    addLevel(const LevelP& level);
      
      void performConsistencyCheck() const;
      void printStatistics() const;
      
   private:
      std::vector<LevelP> d_levels;
      
      Grid(const Grid&);
      Grid& operator=(const Grid&);
   };
   
} // end namespace Uintah

//
// $Log$
// Revision 1.4  2000/04/26 06:48:48  sparker
// Streamlined namespaces
//
// Revision 1.3  2000/04/12 23:00:46  sparker
// Starting problem setup code
// Other compilation fixes
//
// Revision 1.2  2000/03/16 22:07:58  dav
// Added the beginnings of cocoon docs.  Added namespaces.  Did a few other coding standards updates too
//
//

#endif


/*
 * The MIT License
 *
 * Copyright (c) 1997-2021 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "TextDumper.h"
#include "ScalarDiags.h"
#include "VectorDiags.h"
#include "TensorDiags.h"
#include <Core/Grid/Variables/NodeIterator.h>
#include <Core/Grid/Variables/CellIterator.h>

using namespace std;

#define ONEDIM_DIM 2

namespace Uintah {

  TextOpts::TextOpts(Args & args)
  {
    onedim  = args.getLogical("onedim");
    tseries = args.getLogical("tseries");
  }
  //______________________________________________________________________
  //
  TextDumper::TextDumper(DataArchive          * da,
                         string basedir,
                         const TextOpts       & opts,
                         const FieldSelection & flds)
    : FieldDumper(da, basedir), opts_(opts), flds_(flds)
  {
    // set defaults for cout
    cout.setf(ios::scientific,ios::floatfield);
    cout.precision(16);

    // set up a file that contains a list of all the files
    string dirname = this->createDirectory();
    string filelistname = dirname+"/timelist";

    filelist_ = fopen(filelistname.c_str(),"w");
    if (!filelist_) {
      cerr << "Can't open output file " << filelistname << endl;
      abort();
    }
  }
  //______________________________________________________________________
  //
  TextDumper::Step *
  TextDumper::addStep(int timestep, double time, int index)
  {
    const string dirName   = this->dirName(time, timestep);
    DataArchive * da = this->archive();

    cout << " dirName: " << dirName  << endl;

    return scinew Step( da, dirName,
                       timestep, time, index, opts_, flds_);
  }
  //______________________________________________________________________
  //
  void
  TextDumper::addField(string fieldname, const Uintah::TypeDescription * type)
  {
  }
  //______________________________________________________________________
  //
  void
  TextDumper::finishStep(FieldDumper::Step * s)
  {
    fprintf(filelist_, "%10d %16.8g  %20s\n", s->timestep_, s->time_, s->infostr().c_str());
  }

  //______________________________________________________________________
  //
  TextDumper::Step::Step(DataArchive * da, string tsdir,
                         int timestep, double time, int index,
                         const TextOpts & opts, const FieldSelection & flds)
    :
    FieldDumper::Step(tsdir, timestep, time, index),
    da_(da), opts_(opts), flds_(flds)
  {
    //    GridP grid = da_->queryGrid(time);
    GridP grid = da_->queryGrid( index_ );
  }

  //______________________________________________________________________
  //
  static
  bool
  outside(IntVector p, IntVector mn, IntVector mx)
  {
    return  ( p[0]<mn[0] || p[0]>=mx[0] ||
              p[1]<mn[1] || p[1]>=mx[1] ||
              p[2]<mn[2] || p[2]>=mx[2] );
  }

  //______________________________________________________________________
  //
  void
  TextDumper::Step::storeField(string fieldname, const Uintah::TypeDescription * td)
  {
    GridP grid = da_->queryGrid(index_);

    cout << "   " << fieldname << endl;
    const Uintah::TypeDescription* subtype = td->getSubType();

    int nmats = 0;
    // count the materials
    for(int l=0;l<=0;l++) { // FIXME: only first level
      LevelP level = grid->getLevel(l);

      for(Level::const_patch_iterator iter = level->patchesBegin();iter != level->patchesEnd(); iter++) {
        const Patch* patch = *iter;
        ConsecutiveRangeSet matls= da_->queryMaterials(fieldname, patch, index_);

        for(ConsecutiveRangeSet::iterator matlIter = matls.begin();matlIter != matls.end(); matlIter++) {
          int matl = *matlIter;
          if(matl>=nmats){
            nmats = matl+1;
          }
        }
      }
    }

    TensorDiag const * tensor_preop = createTensorOp(flds_);

    //__________________________________
    // only support level 0 for now
    for(int l=0;l<=0;l++) {
      LevelP level = grid->getLevel(l);

      IntVector levelLowIndx;
      IntVector levelHighIndx;
      level->findNodeIndexRange(levelLowIndx, levelHighIndx);

      if(opts_.onedim) {
        IntVector ghostl(-levelLowIndx);
        levelLowIndx[0] += ghostl[0];
        levelHighIndx[0] -= ghostl[0];

        for(int id=0;id<3;id++) {
          if(id!=ONEDIM_DIM) {
            levelLowIndx[id] = (levelHighIndx[id] + levelLowIndx[id])/2;
            levelHighIndx[id] = levelLowIndx[id]+1;
          }
        }
      }

      //__________________________________
      //  Patch loop
      for(Level::const_patch_iterator iter = level->patchesBegin();iter != level->patchesEnd(); iter++){
        const Patch* patch = *iter;

        //__________________________________
        // loop over materials
        ConsecutiveRangeSet matls = da_->queryMaterials(fieldname, patch, index_);

        for(ConsecutiveRangeSet::iterator matlIter = matls.begin(); matlIter != matls.end(); matlIter++) {
          const int matl = *matlIter;

          if( !flds_.wantMaterial(matl) ){
           continue;
          }

          list<ScalarDiag const *> scalarDiagGens = createScalarDiags(td, flds_, tensor_preop);
          list<VectorDiag const *> vectorDiagGens = createVectorDiags(td, flds_, tensor_preop);
          list<TensorDiag const *> tensorDiagGens = createTensorDiags(td, flds_, tensor_preop);
          /*
          cout << " have " << scalarDiagGens.size() << " scalar diagnostics" << endl;
          cout << " have " << vectorDiagGens.size() << " vector diagnostics" << endl;
          cout << " have " << tensorDiagGens.size() << " tensor diagnostics" << endl;
          */

          // loop through requested diagnostics
          list<string> outdiags;
          for(list<ScalarDiag const *>::const_iterator diagit(scalarDiagGens.begin());diagit!=scalarDiagGens.end();diagit++){
            outdiags.push_back( (*diagit)->name() );
          }

          for(list<VectorDiag const *>::const_iterator diagit(vectorDiagGens.begin());diagit!=vectorDiagGens.end();diagit++) {
            outdiags.push_back( (*diagit)->name() );
          }

          for(list<TensorDiag const *>::const_iterator diagit(tensorDiagGens.begin());diagit!=tensorDiagGens.end();diagit++){
            outdiags.push_back( (*diagit)->name() );
          }

          map<string, ofstream *> outfiles;
          map<string, string>     outfieldnames;

          for(list<string>::const_iterator dit(outdiags.begin());dit!=outdiags.end();dit++){
              string outfieldname = fieldname;

              if(*dit!="value") {
                outfieldname += "_";
                outfieldname += *dit;
              }
              outfieldnames[*dit] = outfieldname;

              string fname = fileName(outfieldname, matl, "txt");

              outfiles[*dit];
              if(opts_.tseries || timestep_==1) {
                outfiles[*dit] = new ofstream( fname.c_str() );
                *outfiles[*dit]
                  << "# time = " << time_ << ", field = " << fieldname << ", mat " << matl << " of " << nmats << endl;
              } else {
                outfiles[*dit] = new ofstream( fname.c_str(), ios::app);
                *outfiles[*dit] << "# time = " << time_ << ", field = " << fieldname << ", mat " << matl << " of " << nmats << endl;
              }
            }

          bool no_match = false;

          //__________________________________
          //                         DOUBLE
          for(list<ScalarDiag const *>::const_iterator diagit(scalarDiagGens.begin());diagit!=scalarDiagGens.end();diagit++) {

              ofstream & outfile = *outfiles[(*diagit)->name()];
              //cout << "   " << fileName(outfieldnames[(*diagit)->name()], matl, "txt") << endl;

              outfile.precision(16);

              switch(td->getType()){
              case Uintah::TypeDescription::CCVariable:
                {
                  CCVariable<double> svals;
                  (**diagit)(da_, patch, fieldname, matl, index_, svals);

                  for(CellIterator iter = patch->getCellIterator(); !iter.done(); iter++){

                    IntVector c = *iter;
                    if( outside(c, levelLowIndx, levelHighIndx) ){
                      continue;
                    }

                    Point pos = patch->cellPosition(c);

                    if(opts_.tseries) {
                      outfile << time_ << " ";
                    }

                    outfile << pos(0) << " "
                            << pos(1) << " "
                            << pos(2) << " ";
                    outfile << svals[c] << " "
                            << endl;
                  }
                } break;
              case Uintah::TypeDescription::NCVariable:
                {
                  NCVariable<double> svals;
                  (**diagit)(da_, patch, fieldname, matl, index_, svals);

                  for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++){

                    IntVector n = *iter;
                    if( outside(n, levelLowIndx, levelHighIndx) ){
                     continue;
                    }

                    Point pos = patch->nodePosition(n);

                    if(opts_.tseries){
                      outfile << time_ << " ";
                    }

                    outfile << pos(0) << " "
                            << pos(1) << " "
                            << pos(2) << " ";
                    outfile << svals[n] << " "
                            << endl;
                  }
                } break;
              case Uintah::TypeDescription::ParticleVariable:
                {
                  ParticleVariable<Point> pos;
                  da_->query(pos, "p.x", matl, patch, index_);

                  ParticleSubset* pset = pos.getParticleSubset();

                  ParticleVariable<double> svals;
                  (**diagit)(da_, patch, fieldname, matl, index_, pset, svals);

                  for(ParticleSubset::iterator iter = pset->begin(); iter != pset->end(); iter++) {
                    particleIndex idx = *iter;

                    Point p = pos[idx];

                    if(opts_.tseries){
                      outfile << time_ << " ";
                    }

                    outfile << p(0) << " "
                            << p(1) << " "
                            << p(2) << " ";
                    outfile << (svals[idx]) << " "
                            << endl;
                  }
                } break;
              default:
                no_match = true;
              } // td->getType() switch

            } // scalar diags

            //__________________________________
            //                       VECTOR
            for(list<VectorDiag const *>::const_iterator diagit(vectorDiagGens.begin());diagit!=vectorDiagGens.end();diagit++) {
              ofstream & outfile = *outfiles[(*diagit)->name()];

              outfile.precision(16);
              //cout << "   " << fileName(outfieldnames[(*diagit)->name()], matl, "txt") << endl;

              switch(td->getType()){
              case Uintah::TypeDescription::CCVariable:
                {
                  CCVariable<Vector> vvals;
                  (**diagit)(da_, patch, fieldname, matl, index_, vvals);

                  for(NodeIterator iter = patch->getNodeIterator();!iter.done(); iter++){

                    IntVector c = *iter;
                    if( outside(c, levelLowIndx, levelHighIndx) ){
                      continue;
                    }

                    Point pos = patch->cellPosition(c);
                    if(opts_.tseries){
                      outfile << time_ << " ";
                    }
                    outfile << pos(0) << " "
                            << pos(1) << " "
                            << pos(2) << " ";
                    outfile << vvals[c][0] << " "
                            << vvals[c][1] << " "
                            << vvals[c][2] << " "
                            << endl;
                  }
                } break;
              case Uintah::TypeDescription::NCVariable:
                {
                  NCVariable<Vector> vvals;
                  (**diagit)(da_, patch, fieldname, matl, index_, vvals);

                  for(NodeIterator iter = patch->getNodeIterator(); !iter.done(); iter++){

                    IntVector n = *iter;
                    if( outside(n, levelLowIndx, levelHighIndx)  ){
                      continue;
                    }

                    Point pos = patch->nodePosition(n);

                    if(opts_.tseries){
                      outfile << time_ << " ";
                    }

                    outfile << pos(0) << " "
                            << pos(1) << " "
                            << pos(2) << " ";
                    outfile << vvals[n][0] << " "
                            << vvals[n][1] << " "
                            << vvals[n][2] << " "
                            << endl;
                  }
                } break;
              case Uintah::TypeDescription::ParticleVariable:
                {
                  ParticleVariable<Point> pos;
                  da_->query(pos, "p.x", matl, patch, index_);

                  ParticleSubset* pset = pos.getParticleSubset();

                  ParticleVariable<Vector> vvals;
                  (**diagit)(da_, patch, fieldname, matl, index_, pset, vvals);

                  for(ParticleSubset::iterator iter = pset->begin();iter != pset->end(); iter++) {

                    particleIndex idx = *iter;

                    Point p = pos[idx];

                    if(opts_.tseries){
                      outfile << time_ << " ";
                    }
                    outfile << p(0) << " "
                            << p(1) << " "
                            << p(2) << " ";
                    outfile << vvals[idx][0] << " "
                            << vvals[idx][1] << " "
                            << vvals[idx][2] << " "
                            << endl;
                  }
                } break;
              default:
                no_match = true;
              } // td->getType() switch

            } // vector diag


          //__________________________________
          //                         MATRIX3

          for(list<TensorDiag const *>::const_iterator diagit(tensorDiagGens.begin());diagit!=tensorDiagGens.end();diagit++)
            {
              ofstream & outfile = *outfiles[(*diagit)->name()];
              //cout << "   " << fileName(outfieldnames[(*diagit)->name()], matl, "txt") << endl;
              outfile.precision(16);

              switch(td->getType()){
              case Uintah::TypeDescription::CCVariable:
                {
                  CCVariable<Matrix3> tvals;
                  (**diagit)(da_, patch, fieldname, matl, index_, tvals);

                  for(NodeIterator iter = patch->getNodeIterator();!iter.done(); iter++){
                    IntVector n = *iter;

                    if( outside(n, levelLowIndx, levelHighIndx) ){
                      continue;
                    }

                    Point pos = patch->cellPosition(n);

                    if(opts_.tseries){
                      outfile << time_ << " ";
                    }
                    outfile << pos(0) << " "
                            << pos(1) << " "
                            << pos(2) << " ";
                    outfile << tvals[n](0,0) << " "
                            << tvals[n](0,1) << " "
                            << tvals[n](0,2) << " "
                            << tvals[n](1,0) << " "
                            << tvals[n](1,1) << " "
                            << tvals[n](1,2) << " "
                            << tvals[n](2,0) << " "
                            << tvals[n](2,1) << " "
                            << tvals[n](2,2) << " "
                            << endl;
                  }
                } break;
              case Uintah::TypeDescription::NCVariable:
                {
                  NCVariable<Matrix3> tvals;
                  (**diagit)(da_, patch, fieldname, matl, index_, tvals);

                  for(NodeIterator iter = patch->getNodeIterator();!iter.done(); iter++){
                    IntVector n = *iter;

                    if( outside(n, levelLowIndx, levelHighIndx) ){
                      continue;
                    }

                    Point pos = patch->nodePosition(n);

                    if(opts_.tseries){
                      outfile << time_ << " ";
                    }

                    outfile << pos(0) << " "
                            << pos(1) << " "
                            << pos(2) << " ";
                    outfile << tvals[n](0,0) << " "
                            << tvals[n](0,1) << " "
                            << tvals[n](0,2) << " "
                            << tvals[n](1,0) << " "
                            << tvals[n](1,1) << " "
                            << tvals[n](1,2) << " "
                            << tvals[n](2,0) << " "
                            << tvals[n](2,1) << " "
                            << tvals[n](2,2) << " "
                            << endl;
                  }
                } break;
              case Uintah::TypeDescription::ParticleVariable:
                {
                  ParticleVariable<Point> pos;
                  da_->query(pos, "p.x", matl, patch, index_);

                  ParticleSubset* pset = pos.getParticleSubset();

                  ParticleVariable<Matrix3> tvals;
                  (**diagit)(da_, patch, fieldname, matl, index_, pset, tvals);

                  for(ParticleSubset::iterator iter = pset->begin();iter != pset->end(); iter++) {
                    particleIndex idx = *iter;

                    Point p = pos[idx];

                    if(opts_.tseries){
                      outfile << time_ << " ";
                    }

                    outfile << p(0) << " "
                            << p(1) << " "
                            << p(2) << " ";
                    outfile << tvals[idx](0,0) << " "
                            << tvals[idx](0,1) << " "
                            << tvals[idx](0,2) << " "
                            << tvals[idx](1,0) << " "
                            << tvals[idx](1,1) << " "
                            << tvals[idx](1,2) << " "
                            << tvals[idx](2,0) << " "
                            << tvals[idx](2,1) << " "
                            << tvals[idx](2,2) << " "
                            << endl;
                  }
                } break;
              default:
                no_match = true;
              } // td->getType() switch
            } // tensor diag

            for(map<string, ofstream *>::iterator fit(outfiles.begin());fit!=outfiles.end();fit++){
              delete fit->second;
            }

          if (no_match){
            cerr << "WARNING: Unexpected type for " << td->getName() << " of " << subtype->getName() << endl;
          }
        } // materials
      } // patches
    } // levels
  }


}

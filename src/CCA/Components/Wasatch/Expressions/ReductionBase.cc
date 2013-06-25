
#include "ReductionBase.h"

// ###################################################################
//
//                          Implementation
//
// ###################################################################

ReductionBase::
ReductionBase( const Expr::Tag& resultTag,
              const Expr::Tag& srcTag,
              ReductionEnum reductionName,
              bool printVar )
: Expr::Expression<double>(),
  srcTag_( srcTag ),
  reductionName_( reductionName ),
  printVar_(printVar)
{
  const std::string redVarName = resultTag.name() + "_uintah";
  switch (reductionName_) {
    case ReduceMin:
      reductionVarLabel_ = Uintah::VarLabel::create( redVarName,
                                                    Uintah::ReductionVariable<double, ReductionMinOpT >::getTypeDescription()
                                                    );
      break;
    case ReduceMax:
      reductionVarLabel_ = Uintah::VarLabel::create( redVarName,
                                                    Uintah::ReductionVariable<double, ReductionMaxOpT >::getTypeDescription()
                                                    );
      break;
    case ReduceSum:
      reductionVarLabel_ = Uintah::VarLabel::create( redVarName,
                                                    Uintah::ReductionVariable<double, ReductionSumOpT >::getTypeDescription()
                                                    );
      break;
    default:
      break;
  }
  thisVarLabel_ = Uintah::VarLabel::create( resultTag.name(),
                                           Uintah::PerPatch<double>::getTypeDescription() );
}

//--------------------------------------------------------------------

ReductionBase::
~ReductionBase()
{
  Uintah::VarLabel::destroy( reductionVarLabel_ );
  Uintah::VarLabel::destroy( thisVarLabel_ );
}

//--------------------------------------------------------------------

void
ReductionBase::
advertise_dependents( Expr::ExprDeps& exprDeps )
{}

//--------------------------------------------------------------------

void
ReductionBase::
bind_fields( const Expr::FieldManagerList& fml )
{}

//--------------------------------------------------------------------

void
ReductionBase::
evaluate()
{}

//--------------------------------------------------------------------

ReductionBase::
Builder::Builder( const Expr::Tag& resultTag,
                  const Expr::Tag& srcTag,
                  ReductionEnum reductionName,
                  bool printVar )
: ExpressionBuilder  ( resultTag         ),
  resultTag_         ( resultTag         ),
  srcTag_            ( srcTag            ),
  reductionName_     ( reductionName     ),
  printVar_          ( printVar          )
{}

//--------------------------------------------------------------------

Expr::ExpressionBase*
ReductionBase::
Builder::build() const
{
  return new ReductionBase( resultTag_, srcTag_, reductionName_, printVar_ );
}

//--------------------------------------------------------------------
void
ReductionBase::
schedule_set_reduction_vars( const Uintah::LevelP& level,
                            Uintah::SchedulerP sched,
                            const Uintah::MaterialSet* const materials,
                            const int RKStage )
{
  dbg_red << "scheduling set reduction vars \n";
  // create a Uintah task to populate the reduction variable with values calculated by this expression
  Uintah::Task* reductionVarTask = scinew Uintah::Task( "set reduction variables", this, &ReductionBase::populate_reduction_variable );
  // we require this perpatch variable
  reductionVarTask->requires( Uintah::Task::NewDW, Uintah::VarLabel::find( this->get_tag().name() ), Wasatch::get_uintah_ghost_type<double>() );
  // we compute the reduction on this per patch variable
  reductionVarTask->computes( reductionVarLabel_ );
  sched->addTask( reductionVarTask, sched->getLoadBalancer()->getPerProcessorPatchSet(level), materials );
  
  dbg_red << "scheduling get reduction vars \n";
  // create a uintah task to get the reduced variables and put them back into this expression
  Uintah::Task* getReductionVarTask = scinew Uintah::Task( "get reduction variables", this, &ReductionBase::get_reduction_variable );
  // get the reduction variable value
  getReductionVarTask->requires( Uintah::Task::NewDW, reductionVarLabel_ );
  // modify the value for the perpatch expression
  getReductionVarTask->modifies( thisVarLabel_ );
  sched->addTask( getReductionVarTask, sched->getLoadBalancer()->getPerProcessorPatchSet(level), materials );
}

//--------------------------------------------------------------------

void
ReductionBase::
populate_reduction_variable( const Uintah::ProcessorGroup* const pg,
                            const Uintah::PatchSubset* const patches,
                            const Uintah::MaterialSubset* const materials,
                            Uintah::DataWarehouse* const oldDW,
                            Uintah::DataWarehouse* const newDW )
{
  dbg_red << "populating reduction variables \n";
  for( int ip=0; ip<patches->size(); ++ip ){
    const Uintah::Patch* const patch = patches->get(ip);
    for( int im=0; im<materials->size(); ++im ){
      Uintah::PerPatch< double* > val;
      if (newDW->exists(thisVarLabel_,im,patch)) {
        newDW->           get( val,    thisVarLabel_, im, patch );
        dbg_red << this->get_tag().name() << " patch " << patch->getID() << " val = " << *val << std::endl;
        Uintah::ReductionVariableBase* redcVar = NULL;
        
        switch (reductionName_) {
          case ReduceMin:
            redcVar = scinew Uintah::ReductionVariable<double, Uintah::Reductions::Min<double> >(*val);
            break;
          case ReduceMax:
            redcVar = scinew Uintah::ReductionVariable<double, Uintah::Reductions::Max<double> >(*val);
            break;
          case ReduceSum:
            redcVar = scinew Uintah::ReductionVariable<double, Uintah::Reductions::Sum<double> >(*val);
            break;
          default:
            break;
        }
        
        if (redcVar) {
          newDW->put(*redcVar,reductionVarLabel_);
          delete redcVar;
        }
        
      } else {
        std::cout << "Warning: reduction variable " << this->get_tag().name() << " not found! \n";
      }
    }
  }
}

//--------------------------------------------------------------------

void
ReductionBase::
get_reduction_variable( const Uintah::ProcessorGroup* const pg,
                       const Uintah::PatchSubset* const patches,
                       const Uintah::MaterialSubset* const materials,
                       Uintah::DataWarehouse* const oldDW,
                       Uintah::DataWarehouse* const newDW )
{
  double reducedValue = 0;
  dbg_red << "getting reduction variables \n";
  // grab the reduced variable
  switch (reductionName_) {
    case ReduceMin:
    {
      Uintah::ReductionVariable<double, Uintah::Reductions::Min<double> > val;
      newDW->           get( val,    reductionVarLabel_ );
      reducedValue = val;
    }
      break;
    case ReduceMax:
    {
      Uintah::ReductionVariable<double, Uintah::Reductions::Max<double> > val;
      newDW->           get( val,    reductionVarLabel_ );
      reducedValue = val;
    }
      break;
    case ReduceSum:
    {
      Uintah::ReductionVariable<double, Uintah::Reductions::Sum<double> > val;
      newDW->           get( val,    reductionVarLabel_ );
      reducedValue = val;
    }
      break;      
    default:
      break;
  }

  
  for( int ip=0; ip<patches->size(); ++ip ){
    const Uintah::Patch* const patch = patches->get(ip);
    for( int im=0; im<materials->size(); ++im ){
      
      if (newDW->exists(thisVarLabel_,im,patch)) {
        // now put the reduced variable back into this expression
        Uintah::PerPatch< double* > perPatchVal;
        newDW->get(perPatchVal, thisVarLabel_, im, patch );
        *perPatchVal = reducedValue;
        //dbg_red << this->get_tag().name() << " patch " << patch->getID() << " value = " << *perPatchVal << std::endl;
        dbg_red << this->get_tag().name() << " expression value on patch " << patch->getID() << " after reduction = " << this->value() << std::endl;
      } else {
        std::cout << "variable not found \n";
      }
    }
  }
  
  if ( printVar_ ) proc0cout << this->get_tag().name() << " = " << this->value() << std::endl;
  
}

Expr::TagList ReductionBase::reductionTagList = Expr::TagList();
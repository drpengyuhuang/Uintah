#include <CCA/Components/Arches/PropertyModelsV2/UFromRhoU.h>
#include <CCA/Components/Arches/UPSHelper.h>

using namespace Uintah;
using namespace ArchesCore;

//--------------------------------------------------------------------------------------------------
UFromRhoU::UFromRhoU( std::string task_name, int matl_index ) :
TaskInterface( task_name, matl_index ){}

//--------------------------------------------------------------------------------------------------
UFromRhoU::~UFromRhoU(){}

//--------------------------------------------------------------------------------------------------
void UFromRhoU::problemSetup( ProblemSpecP& db ){

  using namespace Uintah::ArchesCore;

  m_u_vel_name = parse_ups_for_role( UVELOCITY, db, "uVelocitySPBC" );
  m_v_vel_name = parse_ups_for_role( VVELOCITY, db, "vVelocitySPBC" );
  m_w_vel_name = parse_ups_for_role( WVELOCITY, db, "wVelocitySPBC" );

  m_density_name = "density";
  m_xmom = "x-mom";
  m_ymom = "y-mom";
  m_zmom = "z-mom";
  m_eps_name = "cc_volume_fraction";

}

//--------------------------------------------------------------------------------------------------
void UFromRhoU::create_local_labels(){

  register_new_variable<SFCXVariable<double> >( m_u_vel_name );
  register_new_variable<SFCYVariable<double> >( m_v_vel_name );
  register_new_variable<SFCZVariable<double> >( m_w_vel_name );

}

//--------------------------------------------------------------------------------------------------
void UFromRhoU::register_initialize( AVarInfo& variable_registry ){

  typedef ArchesFieldContainer AFC;

  register_variable( m_density_name, AFC::REQUIRES, 1, AFC::NEWDW, variable_registry, _task_name );
  register_variable( m_xmom, AFC::REQUIRES, 0, AFC::NEWDW, variable_registry, _task_name );
  register_variable( m_ymom, AFC::REQUIRES, 0, AFC::NEWDW, variable_registry, _task_name );
  register_variable( m_zmom, AFC::REQUIRES, 0, AFC::NEWDW, variable_registry, _task_name );
  register_variable( m_u_vel_name, AFC::COMPUTES, variable_registry, _task_name );
  register_variable( m_v_vel_name, AFC::COMPUTES, variable_registry, _task_name );
  register_variable( m_w_vel_name, AFC::COMPUTES, variable_registry, _task_name );
  register_variable( m_eps_name, AFC::REQUIRES, 1, AFC::NEWDW, variable_registry, _task_name );

}

//--------------------------------------------------------------------------------------------------
void UFromRhoU::initialize( const Patch* patch, ArchesTaskInfoManager* tsk_info ){

  constCCVariable<double>&    rho = tsk_info->get_const_uintah_field_add<constCCVariable<double> >( m_density_name );
  constCCVariable<double>&    eps = tsk_info->get_const_uintah_field_add<constCCVariable<double> >( m_eps_name );
  constSFCXVariable<double>& xmom = tsk_info->get_const_uintah_field_add<constSFCXVariable<double> >( m_xmom );
  constSFCYVariable<double>& ymom = tsk_info->get_const_uintah_field_add<constSFCYVariable<double> >( m_ymom );
  constSFCZVariable<double>& zmom = tsk_info->get_const_uintah_field_add<constSFCZVariable<double> >( m_zmom );
  SFCXVariable<double>& u = tsk_info->get_uintah_field_add<SFCXVariable<double> >(m_u_vel_name);
  SFCYVariable<double>& v = tsk_info->get_uintah_field_add<SFCYVariable<double> >(m_v_vel_name);
  SFCZVariable<double>& w = tsk_info->get_uintah_field_add<SFCZVariable<double> >(m_w_vel_name);

  IntVector cell_lo = patch->getCellLowIndex();
  IntVector cell_hi = patch->getCellHighIndex();

  GET_FX_BUFFERED_PATCH_RANGE(0,1);
  Uintah::BlockRange x_range( low_fx_patch_range, high_fx_patch_range );
  Uintah::parallel_for( x_range, [&](int i, int j, int k){

    const double rho_face = (rho(i,j,k) + rho(i-1,j,k))/2.;
    const double eps_face = ( (eps(i,j,k) + eps(i-1,j,k))/2. < 1. ) ? 0. : 1.;

    u(i,j,k) = xmom(i,j,k) / rho_face * eps_face;

  });

  if ( patch->getBCType(Patch::xminus) != Patch::Neighbor ){

    IntVector x_cell_lo = cell_lo;
    IntVector x_cell_hi = cell_hi;
    x_cell_lo[0] -= 1;
    x_cell_hi[0] = x_cell_lo[0]+1;
    Uintah::BlockRange bc_x_range( x_cell_lo, x_cell_hi );
    Uintah::parallel_for( bc_x_range, [&](int i, int j, int k){
      u(i,j,k) = u(i+1,j,k);
    });
  }
  if ( patch->getBCType(Patch::xplus) != Patch::Neighbor ){

    IntVector x_cell_lo = cell_lo;
    IntVector x_cell_hi = cell_hi;
    x_cell_hi[0] += 1;
    x_cell_lo[0] = x_cell_hi[0]+1;
    Uintah::BlockRange bc_x_range( x_cell_lo, x_cell_hi );
    Uintah::parallel_for( bc_x_range, [&](int i, int j, int k){
      u(i,j,k) = u(i-1,j,k);
    });
  }

  GET_FY_BUFFERED_PATCH_RANGE(0,1);
  Uintah::BlockRange y_range( low_fy_patch_range, high_fy_patch_range );
  Uintah::parallel_for( y_range, [&](int i, int j, int k){

    const double rho_face = (rho(i,j,k) + rho(i,j-1,k))/2.;
    const double eps_face = ( (eps(i,j,k) + eps(i,j-1,k))/2. < 1. ) ? 0. : 1.;

    v(i,j,k) = ymom(i,j,k) / rho_face * eps_face;

  });

  if ( patch->getBCType(Patch::yminus) != Patch::Neighbor ){

    IntVector y_cell_lo = cell_lo;
    IntVector y_cell_hi = cell_hi;
    y_cell_lo[1] -= 1;
    y_cell_hi[1] = y_cell_lo[1]+1;
    Uintah::BlockRange bc_y_range( y_cell_lo, y_cell_hi );
    Uintah::parallel_for( bc_y_range, [&](int i, int j, int k){
      v(i,j,k) = v(i,j+1,k);
    });
  }
  if ( patch->getBCType(Patch::xplus) != Patch::Neighbor ){

    IntVector y_cell_lo = cell_lo;
    IntVector y_cell_hi = cell_hi;
    y_cell_hi[1] += 1;
    y_cell_lo[1] = y_cell_hi[1]+1;
    Uintah::BlockRange bc_y_range( y_cell_lo, y_cell_hi );
    Uintah::parallel_for( bc_y_range, [&](int i, int j, int k){
      v(i,j,k) = v(i,j-1,k);
    });
  }

  GET_FZ_BUFFERED_PATCH_RANGE(0,1);
  Uintah::BlockRange z_range( low_fz_patch_range, high_fz_patch_range );
  Uintah::parallel_for( z_range, [&](int i, int j, int k){

    const double rho_face = (rho(i,j,k) + rho(i,j,k-1))/2.;
    const double eps_face = ( (eps(i,j,k) + eps(i,j,k-1))/2. < 1. ) ? 0. : 1.;

    w(i,j,k) = zmom(i,j,k) / rho_face * eps_face;

  });

  if ( patch->getBCType(Patch::zminus) != Patch::Neighbor ){

    IntVector z_cell_lo = cell_lo;
    IntVector z_cell_hi = cell_hi;
    z_cell_lo[2] -= 1;
    z_cell_hi[2] = z_cell_lo[2]+1;
    Uintah::BlockRange bc_z_range( z_cell_lo, z_cell_hi );
    Uintah::parallel_for( bc_z_range, [&](int i, int j, int k){
      w(i,j,k) = w(i,j,k+1);
    });
  }
  if ( patch->getBCType(Patch::zplus) != Patch::Neighbor ){

    IntVector z_cell_lo = cell_lo;
    IntVector z_cell_hi = cell_hi;
    z_cell_hi[2] += 1;
    z_cell_lo[2] = z_cell_hi[2]+1;
    Uintah::BlockRange bc_z_range( z_cell_lo, z_cell_hi );
    Uintah::parallel_for( bc_z_range, [&](int i, int j, int k){
      w(i,j,k) = w(i,j,k-1);
    });
  }

}

//--------------------------------------------------------------------------------------------------
void UFromRhoU::register_timestep_eval( VIVec& variable_registry, const int time_substep ){

  typedef ArchesFieldContainer AFC;

  register_variable( m_density_name, AFC::REQUIRES, 1, AFC::NEWDW, variable_registry, _task_name );
  register_variable( m_xmom, AFC::REQUIRES, 0, AFC::NEWDW, variable_registry, _task_name );
  register_variable( m_ymom, AFC::REQUIRES, 0, AFC::NEWDW, variable_registry, _task_name );
  register_variable( m_zmom, AFC::REQUIRES, 0, AFC::NEWDW, variable_registry, _task_name );
  register_variable( m_eps_name, AFC::REQUIRES, 1, AFC::NEWDW, variable_registry, _task_name );
  register_variable( m_u_vel_name, AFC::COMPUTES, variable_registry, _task_name );
  register_variable( m_v_vel_name, AFC::COMPUTES, variable_registry, _task_name );
  register_variable( m_w_vel_name, AFC::COMPUTES, variable_registry, _task_name );

}

//--------------------------------------------------------------------------------------------------
void UFromRhoU::eval( const Patch* patch, ArchesTaskInfoManager* tsk_info ){

  constCCVariable<double>&    rho = tsk_info->get_const_uintah_field_add<constCCVariable<double> >( m_density_name );
  constCCVariable<double>&    eps = tsk_info->get_const_uintah_field_add<constCCVariable<double> >( m_eps_name );
  constSFCXVariable<double>& xmom = tsk_info->get_const_uintah_field_add<constSFCXVariable<double> >( m_xmom );
  constSFCYVariable<double>& ymom = tsk_info->get_const_uintah_field_add<constSFCYVariable<double> >( m_ymom );
  constSFCZVariable<double>& zmom = tsk_info->get_const_uintah_field_add<constSFCZVariable<double> >( m_zmom );
  SFCXVariable<double>& u = tsk_info->get_uintah_field_add<SFCXVariable<double> >(m_u_vel_name);
  SFCYVariable<double>& v = tsk_info->get_uintah_field_add<SFCYVariable<double> >(m_v_vel_name);
  SFCZVariable<double>& w = tsk_info->get_uintah_field_add<SFCZVariable<double> >(m_w_vel_name);

  IntVector cell_lo = patch->getCellLowIndex();
  IntVector cell_hi = patch->getCellHighIndex();

  GET_FX_BUFFERED_PATCH_RANGE(0,1);
  Uintah::BlockRange x_range( low_fx_patch_range, high_fx_patch_range );
  Uintah::parallel_for( x_range, [&](int i, int j, int k){

    const double rho_face = (rho(i,j,k) + rho(i-1,j,k))/2.;
    const double eps_face = ( (eps(i,j,k) + eps(i-1,j,k))/2. < 1. ) ? 0. : 1.;

    u(i,j,k) = xmom(i,j,k) / rho_face * eps_face;

  });

  if ( patch->getBCType(Patch::xminus) != Patch::Neighbor ){

    IntVector x_cell_lo = cell_lo;
    IntVector x_cell_hi = cell_hi;
    x_cell_lo[0] -= 1;
    x_cell_hi[0] = x_cell_lo[0]+1;
    Uintah::BlockRange bc_x_range( x_cell_lo, x_cell_hi );
    Uintah::parallel_for( bc_x_range, [&](int i, int j, int k){
      u(i,j,k) = u(i+1,j,k);
    });
  }
  if ( patch->getBCType(Patch::xplus) != Patch::Neighbor ){

    IntVector x_cell_lo = cell_lo;
    IntVector x_cell_hi = cell_hi;
    x_cell_hi[0] += 1;
    x_cell_lo[0] = x_cell_hi[0]+1;
    Uintah::BlockRange bc_x_range( x_cell_lo, x_cell_hi );
    Uintah::parallel_for( bc_x_range, [&](int i, int j, int k){
      u(i,j,k) = u(i-1,j,k);
    });
  }

  GET_FY_BUFFERED_PATCH_RANGE(0,1);
  Uintah::BlockRange y_range( low_fy_patch_range, high_fy_patch_range );
  Uintah::parallel_for( y_range, [&](int i, int j, int k){

    const double rho_face = (rho(i,j,k) + rho(i,j-1,k))/2.;
    const double eps_face = ( (eps(i,j,k) + eps(i,j-1,k))/2. < 1. ) ? 0. : 1.;

    v(i,j,k) = ymom(i,j,k) / rho_face * eps_face;

  });

  if ( patch->getBCType(Patch::yminus) != Patch::Neighbor ){

    IntVector y_cell_lo = cell_lo;
    IntVector y_cell_hi = cell_hi;
    y_cell_lo[1] -= 1;
    y_cell_hi[1] = y_cell_lo[1]+1;
    Uintah::BlockRange bc_y_range( y_cell_lo, y_cell_hi );
    Uintah::parallel_for( bc_y_range, [&](int i, int j, int k){
      v(i,j,k) = v(i,j+1,k);
    });
  }
  if ( patch->getBCType(Patch::xplus) != Patch::Neighbor ){

    IntVector y_cell_lo = cell_lo;
    IntVector y_cell_hi = cell_hi;
    y_cell_hi[1] += 1;
    y_cell_lo[1] = y_cell_hi[1]+1;
    Uintah::BlockRange bc_y_range( y_cell_lo, y_cell_hi );
    Uintah::parallel_for( bc_y_range, [&](int i, int j, int k){
      v(i,j,k) = v(i,j-1,k);
    });
  }

  GET_FZ_BUFFERED_PATCH_RANGE(0,1);
  Uintah::BlockRange z_range( low_fz_patch_range, high_fz_patch_range );
  Uintah::parallel_for( z_range, [&](int i, int j, int k){

    const double rho_face = (rho(i,j,k) + rho(i,j,k-1))/2.;
    const double eps_face = ( (eps(i,j,k) + eps(i,j,k-1))/2. < 1. ) ? 0. : 1.;

    w(i,j,k) = zmom(i,j,k) / rho_face * eps_face;

  });

  if ( patch->getBCType(Patch::zminus) != Patch::Neighbor ){

    IntVector z_cell_lo = cell_lo;
    IntVector z_cell_hi = cell_hi;
    z_cell_lo[2] -= 1;
    z_cell_hi[2] = z_cell_lo[2]+1;
    Uintah::BlockRange bc_z_range( z_cell_lo, z_cell_hi );
    Uintah::parallel_for( bc_z_range, [&](int i, int j, int k){
      w(i,j,k) = w(i,j,k+1);
    });
  }
  if ( patch->getBCType(Patch::zplus) != Patch::Neighbor ){

    IntVector z_cell_lo = cell_lo;
    IntVector z_cell_hi = cell_hi;
    z_cell_hi[2] += 1;
    z_cell_lo[2] = z_cell_hi[2]+1;
    Uintah::BlockRange bc_z_range( z_cell_lo, z_cell_hi );
    Uintah::parallel_for( bc_z_range, [&](int i, int j, int k){
      w(i,j,k) = w(i,j,k-1);
    });
  }
}
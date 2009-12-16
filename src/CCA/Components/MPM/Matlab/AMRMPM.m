% Reference:  Jin Ma, Honbing Lu,and Ranga Komanduri, "Structured Mesh
% Refinement in Generalized Interpolation Material Point (GIMP) Method
% for Simulation of Dynamic Problems," CMES, vol. 12, no.3, pp. 213-227, 2006
%______________________________________________________________________
                                                         
   
   

function [L2_norm,maxError,NN,NP]=amrmpm(problem_type,CFL,NN)

close all
intwarning on

global d_debugging;
global PPC;
global NSFN;               % number of shape function nodes
d_debugging = problem_type;

[mms] = MMS;                % load mms functions

%  valid options:
%  problem type:  impulsiveBar, oscillator, compaction advectBlock, mms
%  CFL:            0 < cfl < 1, usually 0.2 or so.

% bulletproofing
if (~strcmp(problem_type, 'impulsiveBar')  && ...
    ~strcmp(problem_type, 'oscillator')    && ...
    ~strcmp(problem_type, 'compaction')    && ...
    ~strcmp(problem_type, 'advectBlock')   && ...
    ~strcmp(problem_type, 'mms' ))
  fprintf('ERROR, the problem type is invalid\n');
  fprintf('     The valid types are impulsiveBar, oscillator, advectBlock, compaction\n');
  fprintf('usage:  amrmpm(problem type, cfl, R1_dx)\n');
  return;
end
%__________________________________
% Global variables

PPC     = 2;
E       = 1.0e4;
density = 1.0;
speedSound = sqrt(E/density);

Material = cell(1,1);     % structure that holds material properties
mat.E           = E;
mat.density     = density;
mat.speedSound  = speedSound;
Material{1} = mat;
Material{1}.density

interpolation = 'GIMP';

if( strcmp(interpolation,'GIMP') )
  NSFN    = 3;               % Number of shape function nodes Linear:2, GIMP:3
else
  NSFN    = 2;        
end

t_initial  = 0.0;              % physical time
tstep     = 0;                % timestep
bodyForce = 0;

BigNum     = int32(1e5);
d_smallNum = double(1e-16);

bar_min     = 0.0;
bar_max     = 1;

bar_length  = bar_max - bar_min;

domain       = 1.0;
area         = 1.;
plotSwitch   = 0;
plotInterval = 100;
writeData    = 0;
max_tstep    = BigNum;

% HARDWIRED FOR TESTING
%NN          = 16;
R1_dx       =domain/(NN-1)

if (mod(domain,R1_dx) ~= 0)
  fprintf('ERROR, the dx in Region 1 does not divide into the domain evenly\n');
  fprintf('usage:  amrmpm(problem type, cfl, R1_dx)\n');
  return;
end

[Regions, nRegions,NN] = initializeRegions(domain,PPC,R1_dx,interpolation, d_smallNum);


%  find the number of nodes (NN) and the minimum dx
dx_min = double(BigNum);
for r=1:nRegions
  R = Regions{r};
  NN = NN + R.NN;
  dx_min = min(dx_min,R.dx);
  fprintf( 'region %g, min: %g, \t max: %g \t refineRatio: %g dx: %g, NN: %g\n',r, R.min, R.max, R.refineRatio, R.dx, R.NN)
end


% define the boundary condition nodes
BCNodeL(1)  = 1;
BCNodeR(1)  = NN;

if(strcmp(interpolation,'GIMP'))
  BCNodeL(1) = 1;
  BCNodeL(2) = 2;
  BCNodeR(1) = NN-1;
  BCNodeR(2) = NN;
end

%__________________________________
% compute the zone of influence
% compute the positions of the nodes
Lx      = zeros(NN,2);
nodePos = zeros(NN,1);      % node Position
nodeNum = int32(1);

if(strcmp(interpolation,'GIMP'))    % Boundary condition Node
  nodePos(1) = -R1_dx;
else
  nodePos(1) = 0.0;
end;

for r=1:nRegions
  R = Regions{r};
  % loop over all nodes and set the node position
  for  n=1:R.NN  
    if(nodeNum > 1)
      nodePos(nodeNum) = nodePos(nodeNum-1) + R.dx;
    end
    nodeNum = nodeNum + 1;
  end
end

% compute the zone of influence
Lx(1,1)  = 0.0;
Lx(1,2)  = nodePos(2) - nodePos(1);
Lx(NN,1) = nodePos(NN) - nodePos(NN-1);
Lx(NN,2) = 0.0;

for n=2:NN-1
  Lx(n,1) = nodePos(n) - nodePos(n-1);
  Lx(n,2) = nodePos(n+1) - nodePos(n);
end

% output the regions and the Lx
nn = 1;
for r=1:nRegions
  R = Regions{r};
  fprintf('-------------------------Region %g\n',r);
  for n=1:R.NN
    fprintf( 'Node:  %g, nodePos: %6.5f, \t Lx(1): %6.5f Lx(2): %6.5f\n',nn, nodePos(nn),Lx(nn,1), Lx(nn,2));
    nn = nn + 1;
  end
end


%__________________________________
% create particles
fprintf('Particle Position\n');
ip = 1;

startNode = 1;
if( strcmp(interpolation,'GIMP') )
  startNode = 2;
end

for n=startNode:NN-1
  dx_p = (nodePos(n+1) - nodePos(n) )/double(PPC);
  
  offset = dx_p/2.0;
  
  for p = 1:PPC
    xp_new = nodePos(n) + double(p-1) * dx_p + offset;
    
    if( xp_new >= bar_min && xp_new <= bar_max)
    
      xp(ip) = xp_new;
      
      fprintf('nodePos: %4.5e \t xp(%g) %g \t dx_p: %g \t offset: %g',nodePos(n),ip, xp(ip),dx_p,offset);
      
      if(ip > 1)
        fprintf( '\t \tdx: %g \n',(xp(ip) - xp(ip-1)));
      else
        fprintf('\n');
      end
      
      ip = ip + 1;
      
    end
  end
end


NP=ip-1;  % number of particles

xp = reshape(xp, NP,1);

%__________________________________
% pre-allocate variables for speed
vol       = zeros(NP,1);
lp        = zeros(NP,1);
massP     = zeros(NP,1);
velP      = zeros(NP,1);
dp        = zeros(NP,1);
stressP   = zeros(NP,1);
Fp        = zeros(NP,1);
dF        = zeros(NP,1);
accl_extForceP = zeros(NP,1);

nodes     = zeros(1,NSFN);
Gs        = zeros(1,NSFN);
Ss        = zeros(1,NSFN);

massG     = zeros(NN,1);
momG      = zeros(NN,1);
velG      = zeros(NN,1);
vel_nobc_G= zeros(NN,1);
accl_G    = zeros(NN,1);
extForceG = zeros(NN,1);
intForceG = zeros(NN,1);


KE        = zeros(BigNum,1);
SE        = zeros(BigNum,1);
TE        = zeros(BigNum,1);
totalMom  = zeros(BigNum,1);
Exact_tip = zeros(BigNum,1);
DX_tip    = zeros(BigNum,1);
TIME      = zeros(BigNum,1);
totalEng_err   = zeros(BigNum,1);
totalMom_err   = zeros(BigNum,1);
tipDeflect_err = zeros(BigNum,1);


%__________________________________
% initialize other particle variables
for ip=1:NP
  [volP_0, lp_0] = positionToVolP(xp(ip), nRegions, Regions);
  
  vol(ip)   = volP_0;
  massP(ip) = volP_0*density;
  lp(ip)    = lp_0;
  Fp(ip)    = 1.;                     % total deformation
end

%______________________________________________________________________
% problem specific parameters

if strcmp(problem_type, 'impulsiveBar')
  period          = sqrt(16.*bar_length*bar_length*density/E);
  t_final          = period;
  TipForce        = 10.;
  D               = TipForce*bar_length/(area*E);
  M               = 4.*D/period;
  accl_extForceP(NP)   = TipForce;
  velG_BCValue(1) = 0.;
  velG_BCValue(2) = 1.;
end

if strcmp(problem_type, 'oscillator')
  Mass            = 10000.;
  period          = 2.*3.14159/sqrt(E/Mass);
  t_final          = period;
  v0              = 0.5;
  Amp             = v0/(2.*3.14159/period);
  massP(NP)       = Mass;                    % last particle masss
  velP(NP)        = v0;                      % last particle velocity
  numBCs          = 1;
  velG_BCValueL   = 0.;
  velG_BCValueR   = 1.;
end

if strcmp(problem_type, 'advectBlock')
  initVelocity    = 100;
  t_final          = 0.5;
  numBCs          = 1;
  velG_BCValueL   = initVelocity;
  velG_BCValueR   = initVelocity;
  for ip=1:NP
    velP(ip)    = initVelocity;
    initPos(ip) = xp(ip);
  end
end

if strcmp(problem_type, 'compaction')
  initVelocity    = 0;
  waveTansitTime  = bar_length/speedSound
  t_final          = waveTansitTime * 45;
  numBCs          = 1;
  delta_0         = 50;
  velG_BCValueL   = initVelocity;
  velG_BCValueR   = initVelocity;
  titleStr(1) = {'Quasi-Static Compaction Problem'};
  
end

if strcmp(problem_type, 'mms')
  initVelocity    = 0;
  waveTansitTime  = bar_length/speedSound
  t_initial       = 1.0/(2.0 * speedSound) 
  t_final          = t_initial + waveTansitTime * 1.0;
  numBCs          = 1;
  delta_0         = 0;
  velG_BCValueL   = initVelocity;
  velG_BCValueR   = initVelocity;
  titleStr(1) = {'MMS Problem'};
  
  xp_initial = zeros(NP,1);
  xp_initial = xp;
  [Fp]      = mms.deformationGradient(xp_initial, t_initial, NP,speedSound, bar_length);
  [dp]      = mms.displacement(       xp_initial, t_initial, NP, speedSound, bar_length);
  [velP]    = mms.velocity(           xp_initial, t_initial, NP, speedSound, bar_length);
  [stressP] = computeStress(E,Fp,NP);
  
  lp  = lp .* Fp;
  vol = vol .* Fp;
  xp  =  xp_initial + dp;
end

%__________________________________
titleStr(2) = {sprintf('Computational Domain 0,%g, MPM bar %g,%g',domain,bar_min, bar_max)};
titleStr(3) = {sprintf('%s, PPC: %g',interpolation, PPC)};
%titleStr(4)={'Variable Resolution, Center Region refinement ratio: 2'}
titleStr(4) ={sprintf('Constant resolution, #cells %g', NN)};


%plot initial conditions
if(plotSwitch == 1)
  plotResults(titleStr, t_initial, tstep, xp, dp, massP, Fp, velP, stressP, nodePos, velG, massG, momG)
end
fprintf('t_final: %g, interpolator: %s, NN: %g, NP: %g dx_min: %g \n',t_final,interpolation, NN,NP,dx_min);
%input('hit return')

fn = sprintf('initialConditions.dat');
fid = fopen(fn, 'w');
fprintf(fid,'#p, xp, velP, Fp, stressP accl_extForceP\n');
for ip=1:NP
  fprintf(fid,'%g %16.15E %16.15E %16.15E %16.15E %16.15E\n',ip, xp(ip),velP(ip),Fp(ip), stressP(ip), accl_extForceP(ip));
end
fclose(fid);

%==========================================================================
% Main timstep loop
t = t_initial;
while t<t_final && tstep < max_tstep

  % compute the timestep
  dt = double(BigNum);
  for ip=1:NP
    dt = min(dt, CFL*dx_min/(speedSound + abs(velP(ip) ) ) );
  end

  tstep = tstep + 1;
  t = t + dt;
  if (mod(tstep,20) == 0)
    fprintf('timestep %g, dt = %g, time %g \n',tstep, dt, t)
  end
  
  % initialize arrays to be zero
  for ig=1:NN
    massG(ig)     =1.e-100;
    velG(ig)      =0.;
    vel_nobc_G(ig)=0.;
    accl_G(ig)    =0.;
    extForceG(ig) =0.;
    intForceG(ig) =0.;
  end
  
  % compute the problem specific external force acceleration.
  [accl_extForceP, delta] = ExternalForceAccl(problem_type, delta_0, bodyForce, Material, xp, xp_initial, t, tstep, NP, R1_dx, bar_length);
    
  %__________________________________
  % project particle data to grid  
  for ip=1:NP
  
    [nodes,Ss]=findNodesAndWeights_gimp2(xp(ip), lp(ip), nRegions, Regions, nodePos, Lx);
    for ig=1:NSFN
      massG(nodes(ig))     = massG(nodes(ig))     + massP(ip) * Ss(ig);
      velG(nodes(ig))      = velG(nodes(ig))      + massP(ip) * velP(ip) * Ss(ig);
      extForceG(nodes(ig)) = extForceG(nodes(ig)) + massP(ip) * accl_extForceP(ip) * Ss(ig); 
      
      % debugging__________________________________
      if(0)
        fprintf( 'ip: %g xp: %g, nodes: %g, node_pos: %g massG: %g, massP: %g, Ss: %g,  prod: %g \n', ip, xp(ip), nodes(ig), nodePos(nodes(ig)), massG(nodes(ig)), massP(ip), Ss(ig), massP(ip) * Ss(ig) );
        fprintf( '\t velG:      %g,  velP:       %g,  prod: %g \n', velG(nodes(ig)), velP(ip), (massP(ip) * velP(ip) * Ss(ig) ) );
        fprintf( '\t extForceG: %g,  accl_extForceP:  %g,  prod: %g \n', extForceG(nodes(ig)), accl_extForceP(ip), accl_extForceP(ip) * Ss(ig) );
      end 
      % debugging__________________________________

    end
  end

  % normalize by the mass
  velG = velG./massG;
  vel_nobc_G = velG;

  % set velocity BC
  for ibc=1:length(BCNodeL)
    velG(BCNodeL(ibc)) = velG_BCValueL;
    velG(BCNodeR(ibc)) = velG_BCValueR;
  end

  
  %compute internal force
  for ip=1:NP
    [nodes,Gs,dx]=findNodesAndWeightGradients_gimp2(xp(ip), lp(ip), nRegions, Regions, nodePos,Lx);
    for ig=1:NSFN
      intForceG(nodes(ig)) = intForceG(nodes(ig)) - Gs(ig) * stressP(ip) * vol(ip);
    end
  end

  %__________________________________
  %compute the acceleration and new velocity on the grid
  accl_G    =(intForceG + extForceG)./massG;
  vel_new_G = velG + accl_G.*dt;
  

  %set velocity BC
  for ibc=1:length(BCNodeL)
    vel_new_G(BCNodeL(ibc)) = velG_BCValueL;
    vel_new_G(BCNodeR(ibc)) = velG_BCValueR;
  end

  momG = massG .* vel_new_G;

  %__________________________________
  % compute the acceleration on the grid
  for ig=1:NN
    accl_G(ig)  = (vel_new_G(ig) - vel_nobc_G(ig))/dt;
  end
  
  
  %set acceleration BC
  for ibc=1:length(BCNodeL)
    accl_G(BCNodeL(ibc)) = 0.0;
    accl_G(BCNodeR(ibc)) = 0.0;
  end
 
  
  %compute particle stress
  [Fp, dF,vol,lp] = computeDeformationGradient(xp,lp,dt,vel_new_G,Fp,NP, nRegions, Regions, nodePos,Lx);
  [stressP]       = computeStress(E,Fp,NP);
  
  %__________________________________
  %project changes back to particles
  tmp = zeros(NP,1);
  for ip=1:NP
    [nodes,Ss]=findNodesAndWeights_gimp2(xp(ip), lp(ip), nRegions, Regions, nodePos, Lx);
    dvelP = 0.;
    dxp   = 0.;
    
    for ig=1:NSFN
      dvelP = dvelP + accl_G(nodes(ig))    * dt * Ss(ig);
      dxp   = dxp   + vel_new_G(nodes(ig)) * dt * Ss(ig);
    end

    velP(ip) = velP(ip) + dvelP;
    xp(ip)   = xp(ip) + dxp; 
    dp(ip)   = dp(ip) + dxp;
    tmp(ip)  = tmp(ip) + dxp;
  end
  
 
  
  DX_tip(tstep)=dp(NP);
  T=t; %-dt;

  %__________________________________
  % compute kinetic, strain and total energy
  KE(tstep)=0.;
  SE(tstep)=0.;
  totalMom(tstep) = 0;
  
  for ip=1:NP
    totalMom(tstep) = totalMom(tstep) + massP(ip) * velP(ip);
    KE(tstep) = KE(tstep) + .5*massP(ip) * velP(ip) * velP(ip);
    SE(tstep) = SE(tstep) + .5*stressP(ip) * (Fp(ip)-1.) * vol(ip);
    TE(tstep) = KE(tstep) + SE(tstep);
  end

  %__________________________________
  % Place data into structures
  G.nodePos   = nodePos;  % Grid based Variables
  
  P.velP      = velP;     % particle variables
  P.Fp        = Fp;
  P.xp        = xp;
  P.dp        = dp;
  P.extForceP = accl_extForceP;
  
  OV.speedSound = speedSound;    % Other Variables
  OV.NP         = NP;
  OV.E          = E;
  OV.t          = t;
  OV.tstep      = tstep;
  OV.bar_length = bar_length;
  
  %__________________________________
  % compute the exact solutions
  if strcmp(problem_type, 'impulsiveBar')
    if(T <= period/2.)
      Exact_tip(tstep) = M*T;
    else
      Exact_tip(tstep) = 4.*D-M*T;
    end
  end
  
  if strcmp(problem_type, 'oscillator')
    Exact_tip(tstep) = Amp*sin(2. * 3.14159 * T/period);
  end
  
  if strcmp(problem_type, 'advectBlock')
    Exact_tip(tstep) = DX_tip(tstep);
    
    pos_error  = 0;          % position error
    for ip=1:NP
      exact_pos = (initPos(ip) + t * initVelocity);
      pos_error = pos_error +  xp(ip) - exact_pos;
      %fprintf('xp: %16.15f  exact: %16.15f error %16.15f \n',xp(ip), exact_pos, xp(ip) - exact_pos)
    end
    %fprintf('sum position error %E \n',sum(pos_error))
  end
  
  if( strcmp(problem_type, 'compaction') && (mod(tstep,plotInterval) == 0) && (plotSwitch == 1) )
    term1 = (2.0 * density * bodyForce)/E;
    for ip=1:NP
      term2 = term1 * (delta - xp(ip));
      stressExact(ip) = E *  ( sqrt( term2 + 1.0 ) - 1.0);
    end
    
    figure(2)
    set(2,'position',[1000,100,700,700]);

    plot(xp,stressP,'rd', xp, stressExact, 'b');
    axis([0 50 -10000 0])

    title(titleStr)
    legend('Simulation','Exact')
    xlabel('Position');
    ylabel('Particle stress');

    f_name = sprintf('%g.2.ppm',tstep-1);
    F = getframe(gcf);
    [X,map] = frame2im(F);
    imwrite(X,f_name);

    % compute L2Norm
    d = abs(stressP - stressExact);
    L2_norm = sqrt( sum(d.^2)/length(stressP) )
  end
  
  
  if (strcmp(problem_type, 'mms'))
    [L2_norm, maxError] = mms.plotResults(titleStr, plotSwitch, plotInterval, xp_initial, OV, P, G);
  end
  
  
  TIME(tstep)=t;
  
  %__________________________________
  % plot intantaneous solution
  if (mod(tstep,plotInterval) == 0) && (plotSwitch == 1)
    plotResults(titleStr, t, tstep, xp, dp, massP, Fp, velP, stressP, nodePos, velG, massG, momG)
    %input('hit return');
  end
  
  %__________________________________
  % compute on the errors
  totalEng_err(tstep)   =TE(1)-TE(tstep);                      % total energy
  totalMom_err(tstep)   =totalMom(1) - totalMom(tstep);        % total momentum
  tipDeflect_err(tstep) =abs(DX_tip(tstep)-Exact_tip(tstep));  % tip deflection  
  
  %__________________________________
  % bulletproofing
  % particles can't leave the domain
  for ip=1:NP
    if(xp(ip) >= domain) 
      t = t_final;
      fprintf('\nparticle(%g) position is outside the domain: %g \n',ip,xp(ip))
      fprintf('now exiting the time integration loop\n\n') 
    end
  end
end  % main loop

totalEng_err(tstep)
totalMom_err(tstep)
tipDeflect_err(tstep)


%==========================================================================
%  plot the results
%plotFinalResults(TIME, DX_tip, Exact_tip, TE, problem_type, PPC, NN)

%__________________________________
%  write the results out to files
% particle data

if (writeData == 1)
  fname1 = sprintf('particle_NN_%g_PPC_%g.dat',NN, PPC);
  fid = fopen(fname1, 'w');
  fprintf(fid,'#%s, PPC: %g, NN %g\n',problem_type, PPC, NN);
  fprintf(fid,'#p, xp, velP, Fp, stressP, time\n');
  for ip=1:NP
    fprintf(fid,'%g %16.15E %16.15E %16.15E %16.15E %16.15E\n',ip, xp(ip),velP(ip),Fp(ip), stressP(ip), t);
  end
  fclose(fid);

  % grid data
  fname2 = sprintf('grid_NN_%g_PPC_%g.dat',NN, PPC);
  fid = fopen(fname2, 'w');
  fprintf(fid,'#%s, PPC: %g, NN %g\n',problem_type, PPC, NN);
  fprintf(fid,'#node, xG, massG, velG, extForceG, intForceG, accl_G\n');
  for ig=1:NN
    fprintf(fid,'%g, %16.15f, %16.15f, %16.15f, %16.15f, %16.15f %16.15f\n',ig, nodePos(ig), massG(ig), velG(ig), extForceG(ig), intForceG(ig), accl_G(ig));
  end
  fclose(fid);

  % conserved quantities
  fname3 = sprintf('conserved_NN_%g_PPC_%g.dat',NN, PPC);
  fid = fopen(fname3, 'w');
  fprintf(fid,'#%s, PPC: %g, NN %g\n',problem_type, PPC, NN);
  fprintf(fid,'timesetep, KE, SE, TE, totalMom\n');
  for t=1:length(TIME)
    fprintf(fid,'%16.15f, %16.15f, %16.15f, %16.15f, %16.15f\n',TIME(t), KE(t), SE(t), TE(t), totalMom(t));
  end
  fclose(fid);

  % errors
  fname4 = sprintf('errors_NN_%g_PPC_%g.dat',NN, PPC);
  fid = fopen(fname4, 'w');
  fprintf(fid,'#%s, PPC: %g, NN %g\n',problem_type, PPC, NN);
  fprintf(fid,'#timesetep, totalEng_err, totalMom_err, tipDeflect_err\n');
  for t=1:length(TIME)
    fprintf(fid,'%16.15f, %16.15f, %16.15f, %16.15f\n',TIME(t), totalEng_err(t), totalMom_err(t), tipDeflect_err(t));
  end
  fclose(fid);

  fprintf(' Writing out the data files \n\t %s \n\t %s \n\t %s \n\t %s \n',fname1, fname2, fname3,fname4);
end


end
%______________________________________________________________________
% functions
%______________________________________________________________________
function [Fp, dF, vol, lp] = computeDeformationGradient(xp,lp,dt,velG,Fp,NP, nRegions, Regions, nodePos,Lx)
  global NSFN;
  
  for ip=1:NP
    [nodes,Gs,dx]  = findNodesAndWeightGradients_gimp2(xp(ip), lp(ip), nRegions, Regions, nodePos, Lx);
    [volP_0, lp_0] = positionToVolP(xp(ip), nRegions, Regions);

    gUp=0.0;
    for ig=1:NSFN
      gUp = gUp + velG(nodes(ig)) * Gs(ig);
    end

    dF(ip)      = 1. + gUp * dt;
    Fp(ip)      = dF(ip) * Fp(ip);
    vol(ip)     = volP_0 * Fp(ip);
    lp(ip)      = lp_0 * Fp(ip);
  end
end


%__________________________________
function [stressP]=computeStress(E,Fp,NP)
                                                                                
  for ip=1:NP
%    stressP(ip) = E * (Fp(ip)-1.0);
    stressP(ip) = (E/2.0) * ( Fp(ip) - 1.0/Fp(ip) );        % hardwired for the mms test  see eq 50
  end
end

%__________________________________
%
function[node, dx]=positionToNode(xp, nRegions, Regions)
 
  n_offset = 0;
  region1_offset = 1;                       % only needed for the first region
  dx = double(0);
  node = int32(-9);
  
  for r=1:nRegions
    R = Regions{r};
    
    if ((xp >= R.min) && (xp < R.max))
      n    = floor((xp - R.min)/R.dx);      % # of nodes from the start of the current region
      node = n + n_offset + region1_offset; % add an offset to the local node number
      dx   = R.dx;
      %fprintf( 'region: %g, n: %g, node:%g, xp: %g dx: %g R.min: %g, R.max: %g \n',r, n, node, xp, dx, R.min, R.max);
      return;
    end
    region1_offset = 0;                     % set to 0 after the first region

    n_offset = (n_offset) + R.NN;           % increment the offset
  end
  
  %bulletproofing
 if( xp < Regions{1}.min || xp > Regions{nRegions}.max)
  fprintf( 'ERROR: positionToNode(), the particle (xp: %g) is outside the computational domain( %g, %g )\n',xp,Regions{1}.min,Regions{nRegions}.max  );
  input('stop'); 
 end
end


%__________________________________
function[nodes,dx]=positionToClosestNodes(xp,nRegions,Regions, nodePos)
  [node, dx]=positionToNode(xp, nRegions, Regions);
  
  relativePosition = (xp - nodePos(node))/dx;
  
  offset = int32(0);
  if( relativePosition < 0.5)
    offset = -1;
  end
  
  % bulletproofing
  if(relativePosition< 0)
    fprintf( 'Node %g, offset :%g relative Position: %g, xp:%g, nodePos:%g \n',node, offset, relativePosition,xp, nodePos(node));
    input('stop');
  end
  
  nodes(1) = node + offset;
  nodes(2) = nodes(1) + 1;
  nodes(3) = nodes(2) + 1;
  
end
%__________________________________
% returns the initial volP and lp
function[volP_0, lp_0]=positionToVolP(xp, nRegions, Regions)
  volP_0 = -9.0;
  lp_0 = -9.0;
 
  for r=1:nRegions
    R = Regions{r};
    if ( (xp >= R.min) && (xp < R.max) )
      volP_0 = R.dx;
      lp_0   = R.lp;
    end
  end
end



%__________________________________
%  Equation 14 of "Structured Mesh Refinement in Generalized Interpolation Material Point Method
%  for Simulation of Dynamic Problems"
function [nodes,Ss]=findNodesAndWeights_linear(xp, notused, nRegions, Regions, nodePos, Lx)
  global NSFN;
  % find the nodes that surround the given location and
  % the values of the shape functions for those nodes
  % Assume the grid starts at x=0.  This follows the numenclature
  % of equation 12 of the reference

  [node, dx]=positionToNode(xp, nRegions, Regions);  

  nodes(1)= node;
  nodes(2)= node+1;
  
  for ig=1:NSFN
    Ss(ig) = -9;
    
    Lx_minus = Lx(nodes(ig),1);
    Lx_plus  = Lx(nodes(ig),2);
    delX = xp - nodePos(nodes(ig));

    if (delX <= -Lx_minus)
      Ss(ig) = 0;
    elseif(-Lx_minus <= delX && delX<= 0.0)
      Ss(ig) = 1.0 + delX/Lx_minus;
    elseif(  0 <= delX && delX<= Lx_plus)
      Ss(ig) = 1.0 - delX/Lx_plus;
    elseif( Lx_plus <= delX )
      Ss(ig) = 0;
    end
  end
  %__________________________________
  % bullet proofing
  sum = double(0);
  for ig=1:NSFN
    sum = sum + Ss(ig);
  end
  if ( abs(sum-1.0) > 1e-10)
    fprintf('node(1):%g, node(2):%g ,node(3):%g, xp:%g Ss(1): %g, Ss(2): %g, Ss(3): %g, sum: %g\n',nodes(1),nodes(2),nodes(3), xp, Ss(1), Ss(2), Ss(3), sum)
    input('error: the shape functions (linear) dont sum to 1.0 \n');
  end

  if(0)
    node = xp/dx;
    node=floor(node)+1;

    nodes(1)= node;
    nodes(2)=nodes(1)+1;

    dnode=double(node);

    locx=(xp-dx*(dnode-1))/dx;
    Ss_old(1)=1-locx;
    Ss_old(2)=locx;

    if ( ( Ss(1) ~= Ss_old(1)) || ( Ss(1) ~= Ss_old(1)) )
      fprintf('Ss(1): %g, Ss_old: %g     Ss(2):%g   Ss_old(2):%g \n',Ss(1), Ss_old(1), Ss(2), Ss_old(2));
    end
  end
end


%__________________________________
%  Reference:  Uintah Documentation Chapter 7 MPM, Equation 7.14
function [nodes,Ss]=findNodesAndWeights_gimp(xp, lp, nRegions, Regions, nodePos, Lx)
  global NSFN;

 [nodes,dx]=positionToClosestNodes(xp,nRegions,Regions, nodePos);
  
  L = dx;
  
  for ig=1:NSFN
    Ss(ig) = double(-9);
    delX = xp - nodePos(nodes(ig));

    if ( ((-L-lp) < delX) && (delX <= (-L+lp)) )
      
      Ss(ig) = ( ( L + lp + delX)^2 )/ (4.0*L*lp);
      
    elseif( ((-L+lp) < delX) && (delX <= -lp) )
      
      Ss(ig) = 1 + delX/L;
      
    elseif( (-lp < delX) && (delX <= lp) )
      
      numerator = delX^2 + lp^2;
      Ss(ig) =1.0 - (numerator/(2.0*L*lp));  
    
    elseif( (lp < delX) && (delX <= (L-lp)) )
      
      Ss(ig) = 1 - delX/L;
            
    elseif( ((L-lp) < delX) && (delX <= (L+lp)) )
    
      Ss(ig) = ( ( L + lp - delX)^2 )/ (4.0*L*lp);
    
    else
      Ss(ig) = 0;
    end
  end
  
  %__________________________________
  % bullet proofing
  sum = double(0);
  for ig=1:NSFN
    sum = sum + Ss(ig);
  end
  if ( abs(sum-1.0) > 1e-10)
    fprintf('node(1):%g, node(2):%g ,node(3):%g, xp:%g Ss(1): %g, Ss(2): %g, Ss(3): %g, sum: %g\n',nodes(1),nodes(2),nodes(3), xp, Ss(1), Ss(2), Ss(3), sum)
    input('error: the shape functions dont sum to 1.0 \n');
  end
  
end

%__________________________________
%  Equation 15 of "Structured Mesh Refinement in Generalized Interpolation Material Point Method
%  for Simulation of Dynamic Problems"
function [nodes,Ss]=findNodesAndWeights_gimp2(xp, lp, nRegions, Regions, nodePos, Lx)

  global NSFN;
  % find the nodes that surround the given location and
  % the values of the shape functions for those nodes
  % Assume the grid starts at x=0.  This follows the numenclature
  % of equation 15 of the reference
  [nodes,dx]=positionToClosestNodes(xp,nRegions,Regions, nodePos);
 
  for ig=1:NSFN
    node = nodes(ig);
    Lx_minus = Lx(node,1);
    Lx_plus  = Lx(node,2);

    delX = xp - nodePos(node);
    A = delX - lp;
    B = delX + lp;
    a = max( A, -Lx_minus);
    b = min( B,  Lx_plus);
    
    if (B <= -Lx_minus || A >= Lx_plus)
      
      Ss(ig) = 0;
      tmp = 0;
    elseif( b <= 0 )
    
      t1 = b - a;
      t2 = (b*b - a*a)/(2.0*Lx_minus);
      Ss(ig) = (t1 + t2)/(2.0*lp);
      
      tmp = (b-a+(b*b-a*a)/2/Lx_minus)/2/lp;
    elseif( a >= 0 )
      
      t1 = b - a;
      t2 = (b*b - a*a)/(2.0*Lx_plus);
      Ss(ig) = (t1 - t2)/(2.0*lp);
      
      tmp = (b-a-(b*b-a*a)/2/Lx_plus)/2/lp;
    else
    
      t1 = b - a;
      t2 = (a*a)/(2.0*Lx_minus);
      t3 = (b*b)/(2.0*Lx_plus);
      Ss(ig) = (t1 - t2 - t3)/(2*lp);
      
      tmp = (-a-a*a/2/Lx_minus+b-b*b/2/Lx_plus)/2/lp;
    end
    
    if( abs(tmp - Ss(ig)) > 1e-13)
      fprintf(' Ss: %g  tmp: %g \n', Ss(ig), tmp);
      fprintf( 'Node: %g xp: %g nodePos: %g\n', nodes(ig), xp, nodePos(node));
      fprintf( 'A: %g B: %g, a: %g, b: %g Lx_minus: %g, Lx_plus: %g lp: %g\n', A, B, a, b, Lx_minus, Lx_plus,lp);
      fprintf( '(B <= -Lx_minus || A >= Lx_plus) :%g \n',(B <= -Lx_minus || A >= Lx_plus));
      fprintf( '( b <= 0 ) :%g \n',( b <= 0 ));
      fprintf( '( a >= 0 ) :%g \n',( a >= 0 )); 
      input('error shape functions dont match\n');
    end
  end
  

  
  %__________________________________
  % bullet proofing
  sum = double(0);
  for ig=1:NSFN
    sum = sum + Ss(ig);
  end
  if ( abs(sum-1.0) > 1e-10)
    fprintf('node(1):%g, node(1):%g ,node(3):%g, xp:%g Ss(1): %g, Ss(2): %g, Ss(3): %g, sum: %g\n',nodes(1),nodes(2),nodes(3), xp, Ss(1), Ss(2), Ss(3), sum)
    input('error: the shape functions dont sum to 1.0 \n');
  end
  
  %__________________________________
  % error checking
  % Only turn this on with single resolution grids
  if(0)
  [nodes,Ss_old]=findNodesAndWeights_gimp(xp, nRegions, Regions, nodePos, Lx);
  for ig=1:NSFN
    if ( abs(Ss_old(ig)-Ss(ig)) > 1e-10 )
      fprintf(' The methods (old/new) for computing the shape functions dont match\n');
      fprintf('Node: %g, Ss_old: %g, Ss_new: %g \n',node(ig), Ss_old(ig), Ss(ig));
      input('error: shape functions dont match \n'); 
    end
  end
  end
end

%__________________________________
%  Reference:  Uintah Documentation Chapter 7 MPM, Equation 7.14
function [nodes,Gs, dx]=findNodesAndWeightGradients_linear(xp, notUsed, nRegions, Regions, nodePos, Lx)
 
  % find the nodes that surround the given location and
  % the values of the gradients of the linear shape functions.
  % Assume the grid starts at x=0.

  [node, dx]=positionToNode(xp,nRegions, Regions);

  nodes(1) = node;
  nodes(2) = nodes(1)+1;

  Gs(1) = -1/dx;
  Gs(2) = 1/dx;
end

%__________________________________
%  Reference:  Uintah Documentation Chapter 7 MPM, Equation 7.17
function [nodes,Gs, dx]=findNodesAndWeightGradients_gimp(xp, lp, nRegions, Regions, nodePos,Lx)

  global NSFN;
  % find the nodes that surround the given location and
  % the values of the gradients of the shape functions.
  % Assume the grid starts at x=0.  
  [nodes,dx]=positionToClosestNodes(xp,nRegions,Regions, nodePos);
  
  L  = dx;
  
  for ig=1:NSFN
    Gs(ig) = -9;
    delX = xp - nodePos(nodes(ig));

    if ( ((-L-lp) < delX) && (delX <= (-L+lp)) )
      
      Gs(ig) = ( L + lp + delX )/ (2.0*L*lp);
      
    elseif( ((-L+lp) < delX) && (delX <= -lp) )
      
      Gs(ig) = 1/L;
      
    elseif( (-lp < delX) && (delX <= lp) )
      
      Gs(ig) =-delX/(L*lp);  
    
    elseif( (lp < delX) && (delX <= (L-lp)) )
      
      Gs(ig) = -1/L;
            
    elseif( ( (L-lp) < delX) && (delX <= (L+lp)) )
    
      Gs(ig) = -( L + lp - delX )/ (2.0*L*lp);
    
    else
      Gs(ig) = 0;
    end
  end
  
  %__________________________________
  % bullet proofing
  sum = double(0);
  for ig=1:NSFN
    sum = sum + Gs(ig);
  end
  if ( abs(sum) > 1e-10)
    fprintf('node(1):%g, node(1):%g ,node(3):%g, xp:%g Gs(1): %g, Gs(2): %g, Gs(3): %g, sum: %g\n',nodes(1),nodes(2),nodes(3), xp, Gs(1), Gs(2), Gs(3), sum)
    input('error: the gradient of the shape functions (gimp) dont sum to 1.0 \n');
  end
end

%__________________________________
%  The equations for this function are derived in the hand written notes.  
% The governing equations for the derivation come from equation 15.
function [nodes,Gs, dx]=findNodesAndWeightGradients_gimp2(xp, lp, nRegions, Regions, nodePos,Lx)

  global NSFN;
  
  [nodes,dx]=positionToClosestNodes(xp,nRegions,Regions, nodePos);
  
  for ig=1:NSFN
    Gs(ig) = -9;
      
    node = nodes(ig);                                                       
    Lx_minus = Lx(node,1);                                                  
    Lx_plus  = Lx(node,2);                                                  

    delX = xp - nodePos(node);                                              
    A = delX - lp;                                                          
    B = delX + lp;                                                          
    a = max( A, -Lx_minus);                                                 
    b = min( B,  Lx_plus);                                                  

    if (B <= -Lx_minus || A >= Lx_plus)   %--------------------------     B<= -Lx- & A >= Lx+                                    

      Gs(ig) = 0;                                                           

    elseif( b <= 0 )                      %--------------------------     b <= 0                                                

      if( (B < Lx_plus) && (A > -Lx_minus))

        Gs(ig) = 1/Lx_minus;

      elseif( (B >= Lx_plus) && (A > -Lx_minus) )

        Gs(ig) = (Lx_minus + A)/ (2.0 * Lx_minus * lp);

      elseif( (B < Lx_plus) && (A <= -Lx_minus) )

        Gs(ig) = (Lx_minus + B)/ (2.0 * Lx_minus * lp);      

      else
        Gs(ig) = 0.0;
      end                            

    elseif( a >= 0 )                        %--------------------------    a >= 0                                          

      if( (B < Lx_plus) && (A > -Lx_minus))

        Gs(ig) = -1/Lx_plus;

      elseif( (B >= Lx_plus) && (A > -Lx_minus) )

        Gs(ig) = (-Lx_plus + A)/ (2.0 * Lx_plus * lp);

      elseif( (B < Lx_plus) && (A <= -Lx_minus) )

        Gs(ig) = (Lx_plus - B)/ (2.0 * Lx_plus * lp);      

      else
        Gs(ig) = 0.0;
      end  

    else                                      %--------------------------    other                                                    

       if( (B < Lx_plus) && (A > -Lx_minus))

        Gs(ig) = -A/(2.0 * Lx_minus * lp)  - B/(2.0 * Lx_plus * lp);

      elseif( (B >= Lx_plus) && (A > -Lx_minus) )

        Gs(ig) = (-Lx_minus - A)/(2.0 * Lx_minus * lp);

      elseif( (B < Lx_plus) && (A <= -Lx_minus) )

        Gs(ig) = (Lx_plus - B)/(2.0 * Lx_plus * lp);

      else
        Gs(ig) = 0.0;
      end
    end                                          
  end
  
  %__________________________________
  % bullet proofing
  sum = double(0);
  for ig=1:NSFN
    sum = sum + Gs(ig);
  end
  if ( abs(sum) > 1e-10)
    fprintf('node(1):%g, node(2):%g ,node(3):%g, xp:%g Gs(1): %g, Gs(2): %g, Gs(3): %g, sum: %g\n',nodes(1),nodes(2),nodes(3), xp, Gs(1), Gs(2), Gs(3), sum)
    input('error: the gradient of the shape functions (gimp2) dont sum to 0.0 \n');
  end
end


%__________________________________
function plotResults(titleStr,t, tstep, xp, dp, massP, Fp, velP, stressP, nodePos, velG, massG, momG)

    % plot SimulationState
  figure(1)
  set(1,'position',[50,100,700,700]);
  
  vel_G_inter = interp1(xp,velP,nodePos);
  e = ones(size(nodePos));
  e(:) = 1e100;
  
  subplot(4,1,1),plot(xp,velP,'rd');
  ylim([min(1.1*velP - 1e-3) max(1.1*velP + 1e-3)])
  xlabel('Particle Position');
  ylabel('Particle velocity');
  title(titleStr);
  hold on
  errorbar(nodePos,vel_G_inter, e,'LineStyle','none','Color',[0.8314 0.8157 0.7843]);
  hold off
  %axis([0 50 99 101] )

  subplot(4,1,2),plot(xp,Fp,'rd');
  %axis([0 50 0 2] )
  ylabel('Fp');

  subplot(4,1,3),plot(xp,stressP,'rd');
  %axis([0 50 -1 1] )
  ylabel('Particle stress');
  
  subplot(4,1,4),plot(xp,massP,'rd');
  %axis([0 50 -1 1] )
  ylabel('Particle mass');
  
  
if(0)
  subplot(6,1,4),plot(nodePos, velG,'bx');
  xlabel('NodePos');
  ylabel('grid Vel');
  %axis([0 50 0 101] )

  grad_velG = diff(velG);
  grad_velG(length(velG)) = 0;
  
  for n=2:length(velG)
    grad_velG(n) = grad_velG(n)/(nodePos(n) - nodePos(n-1) );
  end
  subplot(6,1,5),plot(nodePos, grad_velG,'bx');
  ylabel('grad velG');
  xlim([0,40])
  
  %subplot(6,1,5),plot(nodePos, massG,'bx');
  %ylabel('gridMass');
  %axis([0 50 0 1.1] )

  momG = velG .* massG;
  subplot(6,1,6),plot(nodePos, momG,'bx');
  ylabel('gridMom');
  %axis([0 50 0 101] )

  f_name = sprintf('%g.ppm',tstep-1);
  F = getframe(gcf);
  [X,map] = frame2im(F);
  imwrite(X,f_name)
  %input('hit return');
end
end

%__________________________________
function plotFinalResults(TIME, DX_tip, Exact_tip, TE, problem_type, PPC, NN)

  close all;
  set(gcf,'position',[100,100,900,900]);

  % tip displacement vs time
  subplot(3,1,1),plot(TIME,DX_tip,'bx');

  hold on;
  subplot(3,1,1),plot(TIME,Exact_tip,'r-');

  tmp = sprintf('%s, ppc: %g, NN: %g',problem_type, PPC, NN);
  title(tmp);
  legend('Simulation','Exact')
  xlabel('Time [sec]');
  ylabel('Tip Deflection')

  %total energy vs time
  subplot(3,1,2),plot(TIME,TE,'b-');
  ylabel('Total Energy');


  % compute error
  err=abs(DX_tip-Exact_tip);

  subplot(3,1,3),plot(TIME,err,'b-');
  ylabel('Abs(error)')
end


%__________________________________
function [accl_extForceP, delta] = ExternalForceAccl(problem_type, delta_0, bodyForce, Material, xp, xp_initial, t, tstep, NP, R1_dx, bar_length)

  density    = Material{1}.density;
  E          = Material{1}.E;
  speedSound = Material{1}.speedSound;
  
  delta = -9;
  
  if (strcmp(problem_type, 'compaction'))
    if(bodyForce > -200)
      bodyForce = -t * 100;
    end
    
    delta = delta_0 + (density*bodyForce/(2.0 * E) ) * (delta_0 * delta_0);  

    displacement = delta - delta_0;
    W = density * abs(bodyForce) * delta_0; 
    
    if (mod(tstep,100) == 0)
      fprintf('Bodyforce: %g displacement:%g, W: %g\n',bodyForce, displacement/R1_dx, W);                                             
    end
    for ip=1:NP                                                                       
       accl_extForceP(ip) = bodyForce;                                                      
    end                                                                               
  end
  
  if (strcmp(problem_type, 'mms'))
    [mms] = MMS;                % load mms functions
    [accl_extForceP] = mms.accl_bodyForce(xp_initial,t, NP, speedSound, bar_length);
  end
end

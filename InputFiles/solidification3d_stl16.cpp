#include "olb3D.h"
#include "olb3D.hh"   // use only generic version!

using namespace olb;
using namespace olb::descriptors;
using namespace olb::graphics;
using namespace std;

typedef double T;

#define NSDESCRIPTOR D3Q19<POROSITY,VELOCITY_SOLID,FORCE>
#define TDESCRIPTOR D3Q7<VELOCITY,TEMPERATURE>

//Hydrodynamic Sector
const T length = 0.5;
const T diameter = 0.2;
const T radius = diameter/2.;
const T entrance_length = 0.1;
const T exit_length = 0.1;
int N = 100;       // resolution of the model   #50 80 128
const T physDeltaX = diameter/N;      // latticeL
T       tau = 0.51;         // relaxation time
const T hydraulicD = diameter;
const T Re = 500.;
const T rho = 11000.;
const T mu = 2.22e-3;
const T nu = mu/rho;
//const T u_inlet = Re*nu/hydraulicD;
const T u_inlet = 0.0010580000000000001;
//const T nu = 1.0*hydraulicD/Re;

//const T Ste = 0.039;        // Stephan number
const T maxPhysT =4000.;   // simulation time
const T output_frequency = 400.;

//Thermal Sector
const T Tcold_real = 591.38;
const T Tmelt_real = 600.6; //in K
const T Thot_real = 693.8;
const T Tcold = 0.5; //in K
const T Tmelt = (Tmelt_real - Tcold_real)/(Thot_real - Tcold_real) + 0.5; //in K
const T Thot = 1.5; //in K

//const T lambda_s = 30.; // W / m K
const T lambda_s = 16.6; // W / m K
const T lambda_l = 16.6; // W / m K
const T R_lambda =  lambda_s/lambda_l;

//const T cp_real = 138.8; // J / kg K
const T cp_real = 138.8;
const T cp_s = 1.; // J / kg K
const T cp_l = 1.; // J / kg K

T lattice_Hcold = cp_s * Tcold;
T lattice_Hhot = cp_l * Thot;

//for this case, the harmonic mean (cp_ref) is applicable
const T cp_ref = 2.0 * cp_s * cp_l / (cp_s + cp_l); // J / kg K

const T L_real = 23648.6; // J/kg
const T Ste = cp_real*(Thot_real-Tmelt_real)/L_real;
const T L = cp_s * (Thot - Tcold) / Ste; // J / kg


/// Stores geometry information in form of material numbers
//void prepareGeometry(SuperGeometry3D<T>& superGeometry,
//                     ThermalUnitConverter<T, NSDESCRIPTOR, TDESCRIPTOR> const& converter, IndicatorF3D<T>& indicator,
//                                           STLreader<T>& stlReader)
void prepareGeometry( ThermalUnitConverter<T, NSDESCRIPTOR, TDESCRIPTOR> const& converter, IndicatorF3D<T>& indicator,
                      STLreader<T>& stlReader, SuperGeometry3D<T>& superGeometry )
{
  OstreamManager clout( std::cout,"prepareGeometry" );
  clout << "Prepare Geometry ..." << std::endl;

  superGeometry.rename( 0,2,indicator );
  superGeometry.rename( 2,1,stlReader );

  std::vector<T> center1(3,T());
  std::vector<T> center2(3,T());



  center1[0] = -physDeltaX;
  center2[0] = physDeltaX;
  IndicatorCylinder3D<T> inflow (center1,center2,radius);
  superGeometry.rename( 2,3,1, inflow );

  center1[0] = length-physDeltaX;
  center2[0] = length+physDeltaX;
  IndicatorCylinder3D<T> outflow (center1,center2,radius);
  superGeometry.rename( 2,4,1, outflow );

  std::vector<T> extend(3,T());
  extend[0] = length-exit_length-entrance_length+physDeltaX;
  extend[1] = diameter+physDeltaX;
  extend[2] = diameter+physDeltaX;
  std::vector<T> origin(3,T());
  origin[0] = entrance_length-physDeltaX;
  origin[1] = -radius - converter.getPhysLength(1)*0.5;
  origin[2] = -radius - converter.getPhysLength(1)*0.5;
  IndicatorCuboid3D<T> cooledsection(extend, origin);
  superGeometry.rename(2,5,cooledsection);

  // Removes all not needed boundary voxels outside the surface
  //superGeometry.clean();
  superGeometry.checkForErrors();
  //superGeometry.innerClean();
  superGeometry.print();

  clout << "Prepare Geometry ... OK" << std::endl;

}


void prepareLattice( ThermalUnitConverter<T, NSDESCRIPTOR, TDESCRIPTOR> const& converter,
                     SuperLattice3D<T, NSDESCRIPTOR>& NSlattice,
                     SuperLattice3D<T, TDESCRIPTOR>& ADlattice,
                     ForcedBGKdynamics<T, NSDESCRIPTOR> &bulkDynamics,
                     Dynamics<T, TDESCRIPTOR>& advectionDiffusionBulkDynamics,
                     STLreader<T>& stlReader,
                     SuperGeometry3D<T>& superGeometry )
{

    OstreamManager clout(std::cout,"prepareLattice");
    clout << "Prepare Lattice ..." << std::endl;

    T omega  =  converter.getLatticeRelaxationFrequency();
    T Tomega  =  converter.getLatticeThermalRelaxationFrequency();

    // Hydrodynamic begins here
    NSlattice.defineDynamics( superGeometry, 0, &instances::getNoDynamics<T, NSDESCRIPTOR>() );

    //auto bulkIndicator = superGeometry.getMaterialIndicator({1, 4, 5});
    auto bulkIndicator = superGeometry.getMaterialIndicator({1, 3, 4});
    NSlattice.defineDynamics( bulkIndicator, &bulkDynamics );

    // Setting of the boundary conditions
    //NSlattice.defineDynamics( superGeometry, 2, &instances::getBounceBack<T, NSDESCRIPTOR>() );
    //NSlattice.defineDynamics( superGeometry, 5, &instances::getBounceBack<T, NSDESCRIPTOR>() );

    //if boundary conditions are chosen to be local
    setInterpolatedVelocityBoundary<T,NSDESCRIPTOR>(NSlattice, omega, superGeometry,3);
    setInterpolatedPressureBoundary<T,NSDESCRIPTOR>(NSlattice, omega, superGeometry, 4);

    // Bouzidi Block
    NSlattice.defineDynamics( superGeometry,2,&instances::getNoDynamics<T,NSDESCRIPTOR>() );
    NSlattice.defineDynamics( superGeometry,5,&instances::getNoDynamics<T,NSDESCRIPTOR>() );

    //std::vector<T> center1(3,T());
    //std::vector<T> center2(3,T());
    //center1[0] = 0.;
    //center2[0] = length;
    //IndicatorCylinder3D<T> pipe (center1,center2,radius+physDeltaX);
    setBouzidiZeroVelocityBoundary<T,NSDESCRIPTOR>(NSlattice, superGeometry, 2, stlReader);
    setBouzidiZeroVelocityBoundary<T,NSDESCRIPTOR>(NSlattice, superGeometry, 5, stlReader);


    // Initial conditions
    AnalyticalConst3D<T,T> rhoF( 1. );
    std::vector<T> velocity( 3,T( 0 ) );
    AnalyticalConst3D<T,T> uzero( velocity );

    //std::vector<T> origin = { length, 0., 0.};
    //std::vector<T> axis = { 1, 0, 0 };
    //CirclePoiseuille3D<T> u(origin, axis, converter.getCharLatticeVelocity(), radius);

    velocity[0] = converter.getCharLatticeVelocity();
    AnalyticalConst3D<T,T> u( velocity );

    NSlattice.iniEquilibrium( bulkIndicator, rhoF, u );
    NSlattice.defineRhoU( bulkIndicator, rhoF, u );
    //NSlattice.defineRhoU( superGeometry,2 , rhoF, uzero );
    //NSlattice.iniEquilibrium( superGeometry,2 , rhoF, uzero );
    //NSlattice.defineRhoU( superGeometry,5 , rhoF, uzero );
    //NSlattice.iniEquilibrium( superGeometry,5 , rhoF, uzero );

    ////Thermal Begins here
    ADlattice.defineDynamics(superGeometry, 0, &instances::getNoDynamics<T, TDESCRIPTOR>());
    ADlattice.defineDynamics(superGeometry, 4, &instances::getNoDynamics<T, TDESCRIPTOR>());
    //ADlattice.defineDynamics(superGeometry, 2, &instances::getNoDynamics<T, TDESCRIPTOR>());

    ADlattice.defineDynamics(superGeometry.getMaterialIndicator({1,2,3,5}), &advectionDiffusionBulkDynamics);

    setAdvectionDiffusionTemperatureBoundary<T,TDESCRIPTOR>(ADlattice, Tomega, superGeometry, 5);
    //setAdvectionDiffusionTemperatureBoundary<T,TDESCRIPTOR>(ADlattice, Tomega, superGeometry, 3);


    setAdvectionDiffusionEnthalpyBoundary<T,TDESCRIPTOR>(ADlattice, Tomega, superGeometry, 3,
      Tmelt,
      Tmelt,
      cp_s,
      cp_l,
      cp_ref / descriptors::invCs2<T,TDESCRIPTOR>() * (converter.getLatticeThermalRelaxationTime() - 0.5) * R_lambda,
      cp_ref / descriptors::invCs2<T,TDESCRIPTOR>() * (converter.getLatticeThermalRelaxationTime() - 0.5),
      L);

    setAdvectionDiffusionConvectionBoundary<T,TDESCRIPTOR>(ADlattice, superGeometry, 4);

    ADlattice.defineDynamics(superGeometry,2, &instances::getBounceBack<T, TDESCRIPTOR>());

    /// define initial conditions
    AnalyticalConst3D<T,T> T_cold(Tcold);
    AnalyticalConst3D<T,T> T_hot(Thot);
    AnalyticalConst3D<T,T> H_hot(lattice_Hhot+L);

    ADlattice.defineRhoU(superGeometry, 3 , H_hot,u);
    //ADlattice.defineField<descriptors::VELOCITY>(superGeometry, 3, u);
    //ADlattice.defineField<descriptors::TEMPERATURE>(superGeometry, 3, T_hot);

    ADlattice.defineRhoU(superGeometry, 5 , T_cold,uzero);

    ADlattice.iniEquilibrium(superGeometry, 1 , H_hot, u);
    ADlattice.iniEquilibrium(superGeometry, 5 , T_cold, uzero);
    ADlattice.iniEquilibrium(superGeometry, 3 , H_hot, u);
    ADlattice.iniEquilibrium(superGeometry, 4 , T_hot, u);
    ADlattice.iniEquilibrium(superGeometry, 2 , H_hot, uzero);

    /// Make the lattice ready for simulation
    NSlattice.initialize();
    ADlattice.initialize();

    clout << "Prepare Lattice ... OK" << std::endl;
}

void setBoundaryValues( ThermalUnitConverter<T, NSDESCRIPTOR, TDESCRIPTOR> const& converter,
                        SuperLattice3D<T, NSDESCRIPTOR>& NSlattice,
                        SuperLattice3D<T, TDESCRIPTOR>& ADlattice,
                        int iT, SuperGeometry3D<T>& superGeometry)
{

}

void getResults( ThermalUnitConverter<T, NSDESCRIPTOR, TDESCRIPTOR> const& converter,
                 SuperLattice3D<T, NSDESCRIPTOR>& NSlattice,
                 SuperLattice3D<T, TDESCRIPTOR>& ADlattice, int iT,
                 SuperGeometry3D<T>& superGeometry,
                 Timer<T>& timer,
                 bool converged)
{

    OstreamManager clout(std::cout,"getResults");

    SuperVTMwriter3D<T> vtmWriter("solidification3D");
    SuperLatticeGeometry3D<T, NSDESCRIPTOR> geometry(NSlattice, superGeometry);
    SuperLatticeField3D<T, TDESCRIPTOR, VELOCITY> velocity_coupled(ADlattice);
    velocity_coupled.getName() = "velocity_coupled";
    SuperLatticePhysPressure3D<T, NSDESCRIPTOR> pressure(NSlattice, converter);
    SuperLatticePhysVelocity3D<T, NSDESCRIPTOR> velocity( NSlattice, converter );


    SuperLatticeDensity3D<T, TDESCRIPTOR> enthalpy(ADlattice);
    enthalpy.getName() = "enthalpy";
    SuperLatticeField3D<T, NSDESCRIPTOR, POROSITY> liquid_frac(NSlattice);
    liquid_frac.getName() = "liquid fraction";
    SuperLatticeField3D<T, TDESCRIPTOR, TEMPERATURE> temperature(ADlattice);
    temperature.getName() = "temperature";
    SuperLatticeField3D<T, NSDESCRIPTOR, FORCE> force(NSlattice);
    force.getName() = "force";
    vtmWriter.addFunctor( geometry );
    vtmWriter.addFunctor( pressure );
    vtmWriter.addFunctor( velocity );
    vtmWriter.addFunctor( velocity_coupled );
    vtmWriter.addFunctor( enthalpy );
    vtmWriter.addFunctor( liquid_frac );
    vtmWriter.addFunctor( temperature );
    vtmWriter.addFunctor( force );

    //const int vtkIter = converter.getLatticeTime(output_frequency);
    const int vtkIter = int (output_frequency/converter.getPhysDeltaT() );
    if (iT == 0) {
        /// Writes the geometry, cuboid no. and rank no. as vti file for visualization
        SuperLatticeGeometry3D<T, NSDESCRIPTOR> geometry(NSlattice, superGeometry);
        SuperLatticeCuboid3D<T, NSDESCRIPTOR> cuboid(NSlattice);
        SuperLatticeRank3D<T, NSDESCRIPTOR> rank(NSlattice);
        vtmWriter.write(geometry);
        vtmWriter.write(cuboid);
        vtmWriter.write(rank);

        vtmWriter.createMasterFile();
    }

    /// Writes the VTK files
    if (iT % vtkIter == 0 || converged) {

        timer.update(iT);
        timer.printStep();

        /// NSLattice statistics console output
        NSlattice.getStatistics().print(iT,converter.getPhysTime(iT));

        /// ADLattice statistics console output
        ADlattice.getStatistics().print(iT,converter.getPhysTime(iT));

        if ( NSlattice.getStatistics().getAverageRho() != NSlattice.getStatistics().getAverageRho() or ADlattice.getStatistics().getAverageRho() != ADlattice.getStatistics().getAverageRho() ) {
            clout << "simulation diverged! stopping now." << endl;
            exit(1);
        }
        vtmWriter.write(iT);

        //clout << "Checkpointing the system at t=" << iT << std::endl;
        //NSlattice.save( "NSsolidification_4000.checkpoint" );
        //ADlattice.save( "ADsolidification_4000.checkpoint" );
    }
/*
    // Saves lattice data
    if ( iT%converter.getLatticeTime( 1 )==0 && iT>0 ) {
      clout << "Checkpointing the system at t=" << iT << std::endl;
      NSlattice.save( "NSsolidification.checkpoint" );
      ADlattice.save( "ADsolidification.checkpoint" );
      // The data can be reloaded using
      //     sLattice.load("bstep2d.checkpoint");
    }
*/
}

int main( int argc, char* argv[] )
{

  // === 1st Step: Initialization ===
  OstreamManager clout(std::cout,"main");
  olbInit(&argc, &argv);
  singleton::directories().setOutputDir("./tmp/");

  clout << "L is equal to :" << L << std::endl;

  const T physDeltaT = 1 / nu / descriptors::invCs2<T,NSDESCRIPTOR>() * (tau - 0.5) * physDeltaX * physDeltaX ;

  ThermalUnitConverter<T, NSDESCRIPTOR, TDESCRIPTOR> const converter(
      (T) physDeltaX, // physDeltaX
      (T) physDeltaT, // physDeltaT
      (T) hydraulicD, // charPhysLength
      (T) u_inlet, // charPhysVelocity
      (T) nu, // physViscosity
      (T) rho, // physDensity
      (T) lambda_l, // physThermalConductivity
      (T) cp_real, // physSpecificHeatCapacity
      (T) 1.0e-4, // physThermalExpansionCoefficient
      (T) Tcold, // charPhysLowTemperature
      (T) Thot // charPhysHighTemperature
  );
  converter.print();
  clout << "lattice cp " << converter.getLatticeSpecificHeatCapacity(cp_l) << endl;

  // === 2rd Step: Prepare Geometry ===

  //std::vector<T> center1(3,T());
  //std::vector<T> center2(3,T());
  //center2[0] = length;
  //IndicatorCylinder3D<T> pipe(center1,center2,radius+converter.getPhysDeltaX());

  STLreader<T> stlReader( "cylinder0.stl", physDeltaX,1e-3,0,true );
  IndicatorLayer3D<T> extendedDomain( stlReader, physDeltaX );

  // Instantiation of a cuboidGeometry with weights
#ifdef PARALLEL_MODE_MPI
  const int noOfCuboids = singleton::mpi().getSize();
#else
  const int noOfCuboids = 2;
#endif
  //CuboidGeometry3D<T> cuboidGeometry( pipe, converter.getPhysDeltaX(), singleton::mpi().getSize() );
  CuboidGeometry3D<T> cuboidGeometry( extendedDomain, converter.getConversionFactorLength(), noOfCuboids );

  // Instantiation of a loadBalancer
  HeuristicLoadBalancer<T> loadBalancer( cuboidGeometry );

  // Instantiation of a superGeometry
  SuperGeometry3D<T> superGeometry( cuboidGeometry, loadBalancer, 2 );
  //prepareGeometry( superGeometry, converter );
  prepareGeometry( converter, extendedDomain, stlReader, superGeometry );

    /// === 3rd Step: Prepare Lattice ===

    SuperLattice3D<T, TDESCRIPTOR> ADlattice(superGeometry);
    SuperLattice3D<T, NSDESCRIPTOR> NSlattice(superGeometry);

    ForcedPSMBGKdynamics<T, NSDESCRIPTOR> NSbulkDynamics(
        converter.getLatticeRelaxationFrequency(),
        instances::getBulkMomenta<T,NSDESCRIPTOR>());

    TotalEnthalpyAdvectionDiffusionTRTdynamics<T, TDESCRIPTOR> TbulkDynamics (
        converter.getLatticeThermalRelaxationFrequency(),
        instances::getAdvectionDiffusionBulkMomenta<T,TDESCRIPTOR>(),
        1./4.,
        Tmelt,
        Tmelt,
        cp_s,
        cp_l,
        cp_ref / descriptors::invCs2<T,TDESCRIPTOR>() * (converter.getLatticeThermalRelaxationTime() - 0.5) * R_lambda,
        cp_ref / descriptors::invCs2<T,TDESCRIPTOR>() * (converter.getLatticeThermalRelaxationTime() - 0.5),
        L
    );


    std::vector<T> dir{0.0, 1.0, 0.0};
    //T boussinesqForcePrefactor = Ra / pow(T(N),3) * Pr * pow(cp_ref / descriptors::invCs2<T,TDESCRIPTOR>() * (converter.getLatticeThermalRelaxationTime() - 0.5), 2);
    //clout << "boussinesq " << Ra / pow(T(N), 3) * Pr * lambda_l * lambda_l << endl;
    TotalEnthalpyPhaseChangeCouplingGenerator3D<T,NSDESCRIPTOR>
    coupling(0, converter.getLatticeLength(length),
             -1 *converter.getLatticeLength(radius),converter.getLatticeLength(radius),
             -1 *converter.getLatticeLength(radius),converter.getLatticeLength(radius), 0.0, 0.0, 1., dir);


    NSlattice.addLatticeCoupling(superGeometry.getMaterialIndicator({1, 3}), coupling, ADlattice);

    //prepareLattice and setBoundaryConditions


		prepareLattice(converter,
										NSlattice, ADlattice,
										NSbulkDynamics, TbulkDynamics ,
                    stlReader,
										superGeometry );


    /// === 4th Step: Main Loop with Timer ===
    Timer<T> timer(converter.getLatticeTime(maxPhysT), superGeometry.getStatistics().getNvoxel() );
    timer.start();

    //NSlattice.load("NSsolidification_4000.checkpoint");
    //ADlattice.load("ADsolidification_4000.checkpoint");
    //for (std::size_t iT = 0; iT < 1; ++iT) {
    for (std::size_t iT = 0; iT < converter.getLatticeTime(maxPhysT)+1; ++iT) {
        /// === 5th Step: Definition of Initial and Boundary Conditions ===
        setBoundaryValues(converter, NSlattice, ADlattice, iT, superGeometry);

        /// === 6th Step: Collide and Stream Execution ===
        NSlattice.executeCoupling();
        NSlattice.collideAndStream();
        ADlattice.collideAndStream();

        /// === 7th Step: Computation and Output of the Results ===
        getResults(converter, NSlattice, ADlattice, iT, superGeometry, timer, false);
        if (iT == converter.getLatticeTime(maxPhysT)) {
          getResults(converter, NSlattice, ADlattice, iT, superGeometry, timer, true);
        }
    }


  timer.stop();
  timer.printSummary();

}

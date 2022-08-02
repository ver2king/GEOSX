/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2020 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2020 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2020 TotalEnergies
 * Copyright (c) 2019-     GEOSX Contributors
 * All rights reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

/**
 * @file SinglePhaseHybridFVM.cpp
 */

#include "SinglePhaseHybridFVM.hpp"

#include "common/TimingMacros.hpp"
#include "constitutive/ConstitutivePassThru.hpp"
#include "constitutive/fluid/SingleFluidBase.hpp"
#include "fieldSpecification/AquiferBoundaryCondition.hpp"
#include "fieldSpecification/FieldSpecificationManager.hpp"
#include "finiteVolume/HybridMimeticDiscretization.hpp"
#include "finiteVolume/MimeticInnerProductDispatch.hpp"
#include "mainInterface/ProblemManager.hpp"
#include "mesh/mpiCommunications/CommunicationTools.hpp"
#include "physicsSolvers/fluidFlow/SinglePhaseBaseExtrinsicData.hpp"
#include "physicsSolvers/fluidFlow/StencilAccessors.hpp"


/**
 * @namespace the geosx namespace that encapsulates the majority of the code
 */
namespace geosx
{

using namespace dataRepository;
using namespace constitutive;
using namespace singlePhaseHybridFVMKernels;
using namespace mimeticInnerProduct;

SinglePhaseHybridFVM::SinglePhaseHybridFVM( const string & name,
                                            Group * const parent ):
  SinglePhaseBase( name, parent ),
  m_faceDofKey( "" ),
  m_areaRelTol( 1e-8 )
{

  // one cell-centered dof per cell
  m_numDofPerCell = 1;
  m_linearSolverParameters.get().mgr.strategy = LinearSolverParameters::MGR::StrategyType::singlePhaseHybridFVM;

}


void SinglePhaseHybridFVM::registerDataOnMesh( Group & meshBodies )
{

  // 1) Register the cell-centered data
  SinglePhaseBase::registerDataOnMesh( meshBodies );

  // 2) Register the face data
  meshBodies.forSubGroups< MeshBody >( [&] ( MeshBody & meshBody )
  {
    MeshLevel & meshLevel = meshBody.getMeshLevel( 0 );
    FaceManager & faceManager = meshLevel.getFaceManager();

    // primary variables: face pressures at the previous converged time step
    faceManager.registerExtrinsicData< extrinsicMeshData::flow::facePressure_n >( getName() );
  } );
}

void SinglePhaseHybridFVM::initializePreSubGroups()
{
  SinglePhaseBase::initializePreSubGroups();

  DomainPartition & domain = this->getGroupByPath< DomainPartition >( "/Problem/domain" );
  NumericalMethodsManager const & numericalMethodManager = domain.getNumericalMethodManager();
  FiniteVolumeManager const & fvManager = numericalMethodManager.getFiniteVolumeManager();

  GEOSX_THROW_IF( !fvManager.hasGroup< HybridMimeticDiscretization >( m_discretizationName ),
                  catalogName() << " " << getName() <<
                  ": the HybridMimeticDiscretization must be selected with SinglePhaseHybridFVM",
                  InputError );
}

void SinglePhaseHybridFVM::initializePostInitialConditionsPreSubGroups()
{
  GEOSX_MARK_FUNCTION;

  SinglePhaseBase::initializePostInitialConditionsPreSubGroups();

  DomainPartition & domain = this->getGroupByPath< DomainPartition >( "/Problem/domain" );

  forMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                MeshLevel & mesh,
                                                arrayView1d< string const > const & regionNames )
  {
    ElementRegionManager const & elemManager = mesh.getElemManager();
    FaceManager const & faceManager = mesh.getFaceManager();

    // in the flux kernel, we need to make sure that we act only on the target regions
    // for that, we need the following region filter
    for( string const & regionName : regionNames )
    {
      m_regionFilter.insert( elemManager.getRegions().getIndex( regionName ) );
    }

    // check that multipliers are stricly larger than 0, which would work with SinglePhaseFVM, but not with SinglePhaseHybridFVM.
    // To deal with a 0 multiplier, we would just have to skip the corresponding face in the FluxKernel
    arrayView1d< real64 const > const & transMultiplier =
      faceManager.getReference< array1d< real64 > >( viewKeyStruct::transMultiplierString() );

    RAJA::ReduceMin< parallelDeviceReduce, real64 > minVal( 1.0 );
    forAll< parallelDevicePolicy<> >( faceManager.size(), [=] GEOSX_HOST_DEVICE ( localIndex const iface )
    {
      minVal.min( transMultiplier[iface] );
    } );

    GEOSX_THROW_IF_LE_MSG( minVal.get(), 0.0,
                           catalogName() << " " << getName() <<
                           "The transmissibility multipliers used in SinglePhaseHybridFVM must strictly larger than 0.0",
                           std::runtime_error );

    FieldSpecificationManager & fsManager = FieldSpecificationManager::getInstance();
    fsManager.apply( 0.0,
                     mesh,
                     "faceManager",
                     extrinsicMeshData::flow::pressure::key(),
                     [&] ( FieldSpecificationBase const & bc,
                           string const &,
                           SortedArrayView< localIndex const > const &,
                           Group &,
                           string const & )
    {
      GEOSX_LOG_RANK_0( catalogName() << " " << getName() <<
                        "A face Dirichlet boundary condition named " << bc.getName() << " was requested in the XML file. \n"
                                                                                        "This type of boundary condition is not yet supported by SinglePhaseHybridFVM and will be ignored" );

    } );

    fsManager.forSubGroups< AquiferBoundaryCondition >( [&] ( AquiferBoundaryCondition const & bc )
    {
      GEOSX_LOG_RANK_0( catalogName() << " " << getName() <<
                        "An aquifer boundary condition named " << bc.getName() << " was requested in the XML file. \n"
                                                                                  "This type of boundary condition is not yet supported by SinglePhaseHybridFVM and will be ignored" );
    } );
  } );
}

void SinglePhaseHybridFVM::implicitStepSetup( real64 const & time_n,
                                              real64 const & dt,
                                              DomainPartition & domain )
{
  GEOSX_MARK_FUNCTION;

  // setup the cell-centered fields
  SinglePhaseBase::implicitStepSetup( time_n, dt, domain );

  // setup the face fields
  forMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                MeshLevel & mesh,
                                                arrayView1d< string const > const & )
  {
    FaceManager & faceManager = mesh.getFaceManager();

    // get the face-based pressures
    arrayView1d< real64 const > const & facePres =
      faceManager.getExtrinsicData< extrinsicMeshData::flow::facePressure >();
    arrayView1d< real64 > const & facePres_n =
      faceManager.getExtrinsicData< extrinsicMeshData::flow::facePressure_n >();
    facePres_n.setValues< parallelDevicePolicy<> >( facePres );
  } );
}

void SinglePhaseHybridFVM::setupDofs( DomainPartition const & GEOSX_UNUSED_PARAM( domain ),
                                      DofManager & dofManager ) const
{

  // setup the connectivity of elem fields
  // we need Connectivity::Face because of the two-point upwinding
  // in AssembleOneSidedMassFluxes
  dofManager.addField( viewKeyStruct::elemDofFieldString(),
                       FieldLocation::Elem,
                       1,
                       m_meshTargets );

  dofManager.addCoupling( viewKeyStruct::elemDofFieldString(),
                          viewKeyStruct::elemDofFieldString(),
                          DofManager::Connector::Face );

  // setup the connectivity of face fields
  dofManager.addField( extrinsicMeshData::flow::facePressure::key(),
                       FieldLocation::Face,
                       1,
                       m_meshTargets );

  dofManager.addCoupling( extrinsicMeshData::flow::facePressure::key(),
                          extrinsicMeshData::flow::facePressure::key(),
                          DofManager::Connector::Elem );

  // setup coupling between pressure and face pressure
  dofManager.addCoupling( extrinsicMeshData::flow::facePressure::key(),
                          viewKeyStruct::elemDofFieldString(),
                          DofManager::Connector::Elem );
}

void SinglePhaseHybridFVM::assembleFluxTerms( real64 const GEOSX_UNUSED_PARAM( time_n ),
                                              real64 const dt,
                                              DomainPartition const & domain,
                                              DofManager const & dofManager,
                                              CRSMatrixView< real64, globalIndex const > const & localMatrix,
                                              arrayView1d< real64 > const & localRhs )
{
  GEOSX_MARK_FUNCTION;

  NumericalMethodsManager const & numericalMethodManager = domain.getNumericalMethodManager();
  FiniteVolumeManager const & fvManager = numericalMethodManager.getFiniteVolumeManager();
  HybridMimeticDiscretization const & hmDiscretization = fvManager.getHybridMimeticDiscretization( m_discretizationName );
  MimeticInnerProductBase const & mimeticInnerProductBase =
    hmDiscretization.getReference< MimeticInnerProductBase >( HybridMimeticDiscretization::viewKeyStruct::innerProductString() );

  // node data (for transmissibility computation)
  forMeshTargets( domain.getMeshBodies(), [&]( string const &,
                                               MeshLevel const & mesh,
                                               arrayView1d< string const > const & regionNames )
  {
    NodeManager const & nodeManager = mesh.getNodeManager();
    FaceManager const & faceManager = mesh.getFaceManager();

    arrayView2d< real64 const, nodes::REFERENCE_POSITION_USD > const & nodePosition = nodeManager.referencePosition();

    // face data

    // get the face-based DOF numbers for the assembly
    string const faceDofKey = dofManager.getKey( extrinsicMeshData::flow::facePressure::key() );
    arrayView1d< globalIndex const > const & faceDofNumber =
      faceManager.getReference< array1d< globalIndex > >( faceDofKey );
    arrayView1d< integer const > const & faceGhostRank = faceManager.ghostRank();

    // get the element dof numbers for the assembly
    string const & elemDofKey = dofManager.getKey( viewKeyStruct::elemDofFieldString() );
    ElementRegionManager::ElementViewAccessor< arrayView1d< globalIndex const > > elemDofNumber =
      mesh.getElemManager().constructArrayViewAccessor< globalIndex, 1 >( elemDofKey );
    elemDofNumber.setName( getName() + "/accessors/" + elemDofKey );

    // get the face-centered pressures
    arrayView1d< real64 const > const & facePres =
      faceManager.getExtrinsicData< extrinsicMeshData::flow::facePressure >();

    // get the face-centered depth
    arrayView1d< real64 const > const & faceGravCoef =
      faceManager.getExtrinsicData< extrinsicMeshData::flow::gravityCoefficient >();

    // get the face-centered transMultiplier
    //    string const & coeffName = hmDiscretization.getReference< string >( HybridMimeticDiscretization::viewKeyStruct::coeffNameString()
    // );
    arrayView1d< real64 const > const & transMultiplier =
      faceManager.getReference< array1d< real64 > >( viewKeyStruct::transMultiplierString() );

    // get the face-to-nodes connectivity for the transmissibility calculation
    ArrayOfArraysView< localIndex const > const & faceToNodes = faceManager.nodeList().toViewConst();

    arrayView2d< localIndex const > const & elemRegionList    = faceManager.elementRegionList();
    arrayView2d< localIndex const > const & elemSubRegionList = faceManager.elementSubRegionList();
    arrayView2d< localIndex const > const & elemList          = faceManager.elementList();

    // tolerance for transmissibility calculation
    real64 const lengthTolerance = domain.getMeshBody( 0 ).getGlobalLengthScale() * m_areaRelTol;

    StencilAccessors< extrinsicMeshData::flow::mobility,
                      extrinsicMeshData::flow::dMobility_dPressure >
    flowAccessors( mesh.getElemManager(), getName() );

    mesh.getElemManager().forElementSubRegionsComplete< CellElementSubRegion >( regionNames,
                                                                                [&]( localIndex const,
                                                                                     localIndex const er,
                                                                                     localIndex const esr,
                                                                                     ElementRegionBase const &,
                                                                                     CellElementSubRegion const & subRegion )
    {
      string const & fluidName = subRegion.getReference< string >( viewKeyStruct::fluidNamesString() );
      SingleFluidBase const & fluid =
        getConstitutiveModel< SingleFluidBase >( subRegion, fluidName );


      string const & permName = subRegion.getReference< string >( viewKeyStruct::permeabilityNamesString() );
      PermeabilityBase const & permeabilityModel =
        getConstitutiveModel< PermeabilityBase >( subRegion, permName );

      mimeticInnerProductDispatch( mimeticInnerProductBase,
                                   [&] ( auto const mimeticInnerProduct )
      {
        using IP_TYPE = TYPEOFREF( mimeticInnerProduct );

        KernelLaunchSelector< IP_TYPE, FluxKernel >( subRegion.numFacesPerElement(),
                                                     er,
                                                     esr,
                                                     subRegion,
                                                     fluid,
                                                     permeabilityModel,
                                                     m_regionFilter.toViewConst(),
                                                     nodePosition,
                                                     elemRegionList,
                                                     elemSubRegionList,
                                                     elemList,
                                                     faceToNodes,
                                                     faceDofNumber,
                                                     faceGhostRank,
                                                     facePres,
                                                     faceGravCoef,
                                                     transMultiplier,
                                                     flowAccessors.get( extrinsicMeshData::flow::mobility{} ),
                                                     flowAccessors.get( extrinsicMeshData::flow::dMobility_dPressure{} ),
                                                     elemDofNumber.toNestedViewConst(),
                                                     dofManager.rankOffset(),
                                                     lengthTolerance,
                                                     dt,
                                                     localMatrix,
                                                     localRhs );
      } );
    } );
  } );

}

void SinglePhaseHybridFVM::assemblePoroelasticFluxTerms( real64 const time_n,
                                                         real64 const dt,
                                                         DomainPartition const & domain,
                                                         DofManager const & dofManager,
                                                         CRSMatrixView< real64, globalIndex const > const & localMatrix,
                                                         arrayView1d< real64 > const & localRhs,
                                                         string const & jumpDofKey )
{
  GEOSX_UNUSED_VAR ( jumpDofKey );

  assembleFluxTerms( time_n,
                     dt,
                     domain,
                     dofManager,
                     localMatrix,
                     localRhs );
}

void SinglePhaseHybridFVM::assembleHydrofracFluxTerms( real64 const time_n,
                                                       real64 const dt,
                                                       DomainPartition const & domain,
                                                       DofManager const & dofManager,
                                                       CRSMatrixView< real64, globalIndex const > const & localMatrix,
                                                       arrayView1d< real64 > const & localRhs,
                                                       CRSMatrixView< real64, localIndex const > const & dR_dAper )
{
  GEOSX_UNUSED_VAR ( time_n );
  GEOSX_UNUSED_VAR ( dt );
  GEOSX_UNUSED_VAR ( domain );
  GEOSX_UNUSED_VAR ( dofManager );
  GEOSX_UNUSED_VAR ( localMatrix );
  GEOSX_UNUSED_VAR ( localRhs );
  GEOSX_UNUSED_VAR ( dR_dAper );

  GEOSX_ERROR( "Poroelastic fluxes with conforming fractures not yet implemented." );
}

void SinglePhaseHybridFVM::applyBoundaryConditions( real64 const time_n,
                                                    real64 const dt,
                                                    DomainPartition & domain,
                                                    DofManager const & dofManager,
                                                    CRSMatrixView< real64, globalIndex const > const & localMatrix,
                                                    arrayView1d< real64 > const & localRhs )
{
  GEOSX_MARK_FUNCTION;

  SinglePhaseBase::applyBoundaryConditions( time_n, dt, domain, dofManager, localMatrix, localRhs );
}

void SinglePhaseHybridFVM::applyAquiferBC( real64 const time,
                                           real64 const dt,
                                           DomainPartition & domain,
                                           DofManager const & dofManager,
                                           CRSMatrixView< real64, globalIndex const > const & localMatrix,
                                           arrayView1d< real64 > const & localRhs ) const
{
  GEOSX_MARK_FUNCTION;

  GEOSX_UNUSED_VAR( time, dt, dofManager, domain, localMatrix, localRhs );
}

void SinglePhaseHybridFVM::saveAquiferConvergedState( real64 const & time,
                                                      real64 const & dt,
                                                      DomainPartition & domain )
{
  GEOSX_MARK_FUNCTION;

  GEOSX_UNUSED_VAR( time, dt, domain );
}


real64 SinglePhaseHybridFVM::calculateResidualNorm( DomainPartition const & domain,
                                                    DofManager const & dofManager,
                                                    arrayView1d< real64 const > const & localRhs )
{
  GEOSX_MARK_FUNCTION;

  real64 localResidualNorm = 0.0;
  real64 localResidualNormalizer = 0.0;

  globalIndex const rankOffset = dofManager.rankOffset();
  string const elemDofKey = dofManager.getKey( viewKeyStruct::elemDofFieldString() );
  string const faceDofKey = dofManager.getKey( extrinsicMeshData::flow::facePressure::key() );

  forMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                MeshLevel const & mesh,
                                                arrayView1d< string const > const & regionNames )
  {

    ElementRegionManager const & elemManager = mesh.getElemManager();
    FaceManager const & faceManager = mesh.getFaceManager();
    real64 defaultViscosity = 0;
    integer subRegionCounter = 0;

    // step 1: compute the residual for the element-based mass conservation equations

    elemManager.forElementSubRegions< ElementSubRegionBase >( regionNames,
                                                              [&]( localIndex const,
                                                                   ElementSubRegionBase const & subRegion )
    {
      real64 subRegionResidualNorm[1]{};
      real64 subRegionResidualNormalizer[1]{};

      string const & fluidName = subRegion.getReference< string >( viewKeyStruct::fluidNamesString() );
      SingleFluidBase const & fluid = getConstitutiveModel< SingleFluidBase >( subRegion, fluidName );
      defaultViscosity += fluid.defaultViscosity();
      subRegionCounter++;

      string const & solidName = subRegion.getReference< string >( viewKeyStruct::solidNamesString() );
      CoupledSolidBase const & solid = getConstitutiveModel< CoupledSolidBase >( subRegion, solidName );

      // step 1.1: compute the norm in the subRegion

      singlePhaseBaseKernels::
        ResidualNormKernelFactory::
        createAndLaunch< parallelDevicePolicy<> >( m_normType,
                                                   rankOffset,
                                                   elemDofKey,
                                                   localRhs,
                                                   subRegion,
                                                   fluid,
                                                   solid,
                                                   subRegionResidualNorm,
                                                   subRegionResidualNormalizer );

      // step 1.2: reduction across meshBodies/regions/subRegions

      if( m_normType == solverBaseKernels::NormType::Linf )
      {
        if( subRegionResidualNorm[0] > localResidualNorm )
        {
          localResidualNorm = subRegionResidualNorm[0];
        }
      }
      else
      {
        localResidualNorm += subRegionResidualNorm[0];
        localResidualNormalizer += subRegionResidualNormalizer[0];
      }

    } );


    // step 2: compute the residual for the face-based constraints

    real64 faceResidualNorm[1]{};
    real64 faceResidualNormalizer[1]{};
    defaultViscosity /= subRegionCounter;

    // step 2.1: compute the norm for the local faces

    real64 dt = 0; /////////////// FRANCOIS

    singlePhaseHybridFVMKernels::
      ResidualNormKernelFactory::
      createAndLaunch< parallelDevicePolicy<> >( m_normType,
                                                 rankOffset,
                                                 faceDofKey,
                                                 localRhs,
                                                 getName(),
                                                 elemManager,
                                                 faceManager,
                                                 defaultViscosity,
                                                 dt,
                                                 faceResidualNorm,
                                                 faceResidualNormalizer );

    // step 2.2: reduction across meshBodies/regions/subRegions

    if( m_normType == solverBaseKernels::NormType::Linf )
    {
      if( faceResidualNorm[0] > localResidualNorm )
      {
        localResidualNorm = faceResidualNorm[0];
      }
    }
    else
    {
      localResidualNorm += faceResidualNorm[0];
      localResidualNormalizer += faceResidualNormalizer[0];
    }
  } );

  // step 3: second reduction across MPI ranks

  real64 const residualNorm = ( m_normType == solverBaseKernels::NormType::Linf )
    ? MpiWrapper::max( localResidualNorm )
    : sqrt( MpiWrapper::sum( localResidualNorm ) ) / MpiWrapper::sum( localResidualNormalizer );

  if( getLogLevel() >= 1 && logger::internal::rank == 0 )
  {
    GEOSX_FMT( "    ( R{} ) = ( {:4.2e} ) ; ", coupledSolverAttributePrefix(), residualNorm );
  }

  return residualNorm;
}


bool SinglePhaseHybridFVM::checkSystemSolution( DomainPartition const & domain,
                                                DofManager const & dofManager,
                                                arrayView1d< real64 const > const & localSolution,
                                                real64 const scalingFactor )
{
  localIndex localCheck = 1;

  string const elemDofKey = dofManager.getKey( viewKeyStruct::elemDofFieldString() );
  string const faceDofKey = dofManager.getKey( extrinsicMeshData::flow::facePressure::key() );

  globalIndex const rankOffset = dofManager.rankOffset();

  forMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                MeshLevel const & mesh,
                                                arrayView1d< string const > const & regionNames )
  {
    FaceManager const & faceManager = mesh.getFaceManager();
    mesh.getElemManager().forElementSubRegions< ElementSubRegionBase >( regionNames, [&]( localIndex const,
                                                                                          ElementSubRegionBase const & subRegion )
    {
      arrayView1d< globalIndex const > const & elemDofNumber =
        subRegion.getReference< array1d< globalIndex > >( elemDofKey );
      arrayView1d< integer const > const & elemGhostRank =
        subRegion.ghostRank();

      arrayView1d< real64 const > const & pres =
        subRegion.getExtrinsicData< extrinsicMeshData::flow::pressure >();

      localIndex const subRegionSolutionCheck =
        singlePhaseBaseKernels::
          SolutionCheckKernel::launch< parallelDevicePolicy<> >( localSolution,
                                                                 rankOffset,
                                                                 elemDofNumber,
                                                                 elemGhostRank,
                                                                 pres,
                                                                 scalingFactor );

      if( subRegionSolutionCheck == 0 )
      {
        localCheck = 0;
      }

    } );

    arrayView1d< integer const > const & faceGhostRank = faceManager.ghostRank();
    arrayView1d< globalIndex const > const & faceDofNumber =
      faceManager.getReference< array1d< globalIndex > >( faceDofKey );

    arrayView1d< real64 const > const & facePres =
      faceManager.getExtrinsicData< extrinsicMeshData::flow::facePressure >();

    localIndex const faceSolutionCheck =
      singlePhaseBaseKernels::
        SolutionCheckKernel::launch< parallelDevicePolicy<> >( localSolution,
                                                               rankOffset,
                                                               faceDofNumber,
                                                               faceGhostRank,
                                                               facePres,
                                                               scalingFactor );

    if( faceSolutionCheck == 0 )
    {
      localCheck = 0;
    }

  } );

  return MpiWrapper::min( localCheck, MPI_COMM_GEOSX );
}


void SinglePhaseHybridFVM::applySystemSolution( DofManager const & dofManager,
                                                arrayView1d< real64 const > const & localSolution,
                                                real64 const scalingFactor,
                                                DomainPartition & domain )
{
  // here we apply the cell-centered update in the derived class
  // to avoid duplicating a synchronization point

  // 1. apply the cell-centered update

  dofManager.addVectorToField( localSolution,
                               viewKeyStruct::elemDofFieldString(),
                               extrinsicMeshData::flow::pressure::key(),
                               scalingFactor );

  // 2. apply the face-based update

  dofManager.addVectorToField( localSolution,
                               extrinsicMeshData::flow::facePressure::key(),
                               extrinsicMeshData::flow::facePressure::key(),
                               scalingFactor );

  // 3. synchronize
  forMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                MeshLevel & mesh,
                                                arrayView1d< string const > const & regionNames )
  {
    FieldIdentifiers fieldsToBeSync;

    fieldsToBeSync.addElementFields( { extrinsicMeshData::flow::pressure::key() }, regionNames );
    fieldsToBeSync.addFields( FieldLocation::Face, { extrinsicMeshData::flow::facePressure::key() } );

    CommunicationTools::getInstance().synchronizeFields( fieldsToBeSync, mesh, domain.getNeighbors(), true );
  } );
}


void SinglePhaseHybridFVM::resetStateToBeginningOfStep( DomainPartition & domain )
{
  // 1. Reset the cell-centered fields
  SinglePhaseBase::resetStateToBeginningOfStep( domain );

  // 2. Reset the face-based fields
  forMeshTargets( domain.getMeshBodies(), [&] ( string const &,
                                                MeshLevel & mesh,
                                                arrayView1d< string const > const & )
  {
    FaceManager & faceManager = mesh.getFaceManager();

    // get the face pressure update
    arrayView1d< real64 > const & facePres =
      faceManager.getExtrinsicData< extrinsicMeshData::flow::facePressure >();
    arrayView1d< real64 const > const & facePres_n =
      faceManager.getExtrinsicData< extrinsicMeshData::flow::facePressure_n >();
    facePres.setValues< parallelDevicePolicy<> >( facePres_n );
  } );
}

REGISTER_CATALOG_ENTRY( SolverBase, SinglePhaseHybridFVM, string const &, Group * const )
} /* namespace geosx */

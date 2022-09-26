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
 * @file Damage.hpp
 * @brief Overrides the SSLE constitutive updates to account for a damage varible and a volumtric-deviatoric split.
 */

#ifndef GEOSX_CONSTITUTIVE_SOLID_DAMAGEVOLDEV_HPP_
#define GEOSX_CONSTITUTIVE_SOLID_DAMAGEVOLDEV_HPP_
#include "Damage.hpp"
#include "InvariantDecompositions.hpp"
#include "SolidBase.hpp"

namespace geosx
{
namespace constitutive
{

/**
 * @class DamageVolDevUpdates
 *
 * @tparam UPDATE_BASE the underlying intact solid model
 *
 * this class implements the material updates for the case of a damage response
 * that is assymetric in tension and compression. The volumetric-deviatoric
 * decomposition of the strain tensor is used to effect the assymetry.
 *
 * References: (for the vol-dev split)
 *
 * Amor, H., Marigo, J.-J., Maurini, C., 2009. Regularized formulation of the variational brittle fracture with unilateral
 * contact: Numerical experiments. Journal of the Mechanics and Physics of Solids 57 (8), 1209 – 1229.
 *
 */
template< typename UPDATE_BASE >
class DamageVolDevUpdates : public DamageUpdates< UPDATE_BASE >
{
public:
  template< typename ... PARAMS >
  DamageVolDevUpdates( arrayView2d< real64 > const & inputDamage,
                       arrayView2d< real64 > const & inputStrainEnergyDensity,
                       real64 const & inputLengthScale,
                       real64 const & inputCriticalFractureEnergy,
                       real64 const & inputcriticalStrainEnergy,
                       PARAMS && ... baseParams ):
    DamageUpdates< UPDATE_BASE >( inputDamage, inputStrainEnergyDensity, inputLengthScale,
                                  inputCriticalFractureEnergy, inputcriticalStrainEnergy,
                                  std::forward< PARAMS >( baseParams )... )
  {}

  using DiscretizationOps = typename DamageUpdates< UPDATE_BASE >::DiscretizationOps;

  using DamageUpdates< UPDATE_BASE >::smallStrainUpdate;
  using DamageUpdates< UPDATE_BASE >::saveConvergedState;

  using DamageUpdates< UPDATE_BASE >::getDegradationValue;
  using DamageUpdates< UPDATE_BASE >::getDegradationDerivative;
  using DamageUpdates< UPDATE_BASE >::getDegradationSecondDerivative;
  using DamageUpdates< UPDATE_BASE >::getEnergyThreshold;

  using DamageUpdates< UPDATE_BASE >::m_strainEnergyDensity;
  using DamageUpdates< UPDATE_BASE >::m_criticalStrainEnergy;
  using DamageUpdates< UPDATE_BASE >::m_criticalFractureEnergy;
  using DamageUpdates< UPDATE_BASE >::m_lengthScale;
  using DamageUpdates< UPDATE_BASE >::m_disableInelasticity;

  ///this function implements the modified stress update to account for the volumetric-deviatori split - it also computes the stiffness and
  /// active part of the strain energy
  /**
   * @brief performs the update of stresses and stiffness, using damage with volumetric-deviatoric decomposition
   *
   * This function performs the updates of the mechanical state of a damaged solid, assuming the vol-dev decomposition
   * for assymetric damage behavior in tension and compression. From the strain increment computed in the solver, it
   * updates the stresses and stiffness of the material at each quadrature point.
   *
   * @note The active part of the strain energy density, stored in m_strainEnergyDensity is also updated in this step.
   * This active part depends on the strain decomposition, since only the positive part of strain contributes to damage
   * evolution.
   *
   * @param[in] k The element index.
   * @param[in] q The quadrature point index.
   * @param[in] strainIncrement Strain increment in Voight notation (linearized strain)
   * @param[out] stress New stress value (Cauchy stress)
   * @param[out] stiffness New stiffness value
   */
  GEOSX_HOST_DEVICE
  virtual void smallStrainUpdate( localIndex const k,
                                  localIndex const q,
                                  real64 const ( &strainIncrement )[6],
                                  real64 ( & stress )[6],
                                  DiscretizationOps & stiffness ) const final
  {
    // perform elastic update for "undamaged" stress

    UPDATE_BASE::smallStrainUpdate( k, q, strainIncrement, stress, stiffness );  // elastic trial update

    if( m_disableInelasticity )
    {
      return;
    }

    // compute volumetric and deviatoric strain invariants

    real64 strain[6];
    UPDATE_BASE::getElasticStrain( k, q, strain );

    real64 volStrain;
    real64 devStrain;
    real64 deviator[6];

    twoInvariant::strainDecomposition( strain,
                                       volStrain,
                                       devStrain,
                                       deviator );

    // degrade shear stiffness always
    // degrade volumetric stiffness when in tension

    real64 factor = getDegradationValue( k, q );

    stiffness.m_shearModulus *= factor;

    if( volStrain > 0 )
    {
      stiffness.m_bulkModulus *= factor;
    }

    // compute stress invariants and recompose full stress tensor

    real64 stressP = stiffness.m_bulkModulus * volStrain;
    real64 stressQ = 3 * stiffness.m_shearModulus * devStrain;

    twoInvariant::stressRecomposition( stressP,
                                       stressQ,
                                       deviator,
                                       stress );

    // update strain energy density
    // TODO: refactor as a proper history variable update.  the code below doesn't allow for rewinds.

    real64 sed = 0.5 * (stressQ * devStrain) / factor;

    if( volStrain > 0 )
    {
      sed += 0.5 * (stressP * volStrain) / factor;
    }

    if( sed > m_strainEnergyDensity( k, q ) )
    {
      m_strainEnergyDensity( k, q ) = sed;
    }
  }

  ///accessor for strain energy density - in this case, it only cares about the positive part
  GEOSX_HOST_DEVICE
  virtual real64 getStrainEnergyDensity( localIndex const k,
                                         localIndex const q ) const override final
  {
    return m_strainEnergyDensity( k, q );
  }

};

/**
 * @class DamageVolDev
 *
 * This class implements the changes to the update functions that are needed
 * to account for an assymetric damage response in tension and compression.
 * In this case, the split between tension and compression is effected through the
 * use of a volumetric-deviatoric decomposition of the strain tensor. Only the positive
 * part of the strain tensor in this decomposition will be degraded. Also, only this
 * part will contribute to the active part of the strain energy that drives damage
 * evolution.
 *
 */
template< typename BASE >
class DamageVolDev : public Damage< BASE >
{
public:

  using KernelWrapper = DamageVolDevUpdates< typename BASE::KernelWrapper >;

  using Damage< BASE >::m_damage;
  using Damage< BASE >::m_strainEnergyDensity;
  using Damage< BASE >::m_criticalFractureEnergy;
  using Damage< BASE >::m_lengthScale;
  using Damage< BASE >::m_criticalStrainEnergy;

  DamageVolDev( string const & name, dataRepository::Group * const parent );
  virtual ~DamageVolDev() override;


  static string catalogName() { return string( "DamageVolDev" ) + BASE::m_catalogNameString; }
  virtual string getCatalogName() const override { return catalogName(); }


  KernelWrapper createKernelUpdates() const
  {
    return BASE::template createDerivedKernelUpdates< KernelWrapper >( m_damage.toView(),
                                                                       m_strainEnergyDensity.toView(),
                                                                       m_lengthScale,
                                                                       m_criticalFractureEnergy,
                                                                       m_criticalStrainEnergy );
  }

};


}
} /* namespace geosx */

#endif /* GEOSX_CONSTITUTIVE_SOLID_DAMAGEVOLDEV_HPP_ */

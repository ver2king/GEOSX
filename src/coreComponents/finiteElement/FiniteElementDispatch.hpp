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
 * @file FiniteElementDispatch.hpp
 */

#ifndef GEOSX_FINITEELEMENT_FINITEELEMENTDISPATCH_HPP_
#define GEOSX_FINITEELEMENT_FINITEELEMENTDISPATCH_HPP_


#include <functional>
#include "elementFormulations/FiniteElementBase.hpp"
#include "elementFormulations/ConformingVirtualElementOrder1.hpp"
#include "elementFormulations/H1_Hexahedron_Lagrange1_GaussLegendre2.hpp"
#include "elementFormulations/H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG1.hpp"
#include "elementFormulations/H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG2.hpp"
#include "elementFormulations/H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG3.hpp"
#include "elementFormulations/H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG4.hpp"
#include "elementFormulations/H1_Pyramid_Lagrange1_Gauss5.hpp"
#include "elementFormulations/H1_QuadrilateralFace_Lagrange1_GaussLegendre2.hpp"
#include "elementFormulations/H1_Tetrahedron_Lagrange1_Gauss1.hpp"
#include "elementFormulations/H1_TriangleFace_Lagrange1_Gauss1.hpp"
#include "elementFormulations/H1_Wedge_Lagrange1_Gauss6.hpp"
#include "elementFormulations/Q3_Hexahedron_Lagrange_GaussLobatto.hpp"
#include "LvArray/src/system.hpp"

#define IF_WRAPPER_CONST( FE ) if (auto const * const ptr = dynamic_cast< FE const * >(&input) ) { std::function< void( FE const & ) > func = [ &lambda ] ( FE const & arg ) { lambda( arg ); }; dispatch3DImpl(*ptr, func );}
#define IF_WRAPPER( FE ) if (auto * const ptr = dynamic_cast< FE * >(&input) ) { std::function< void( FE & ) > func = [ &lambda ] ( FE & arg ) { lambda( arg ); }; dispatch3DImpl(*ptr, func );}
#define IF_WRAPPER_CONST_2D( FE ) if (auto const * const ptr = dynamic_cast< FE const * >(&input) ) { std::function< void( FE const & ) > func = [ &lambda ] ( FE const & arg ) { lambda( arg ); }; dispatch2DImpl(*ptr, func );}

namespace geosx
{
namespace finiteElement
{
//template< typename FE_TYPE >
//void
//dispatch3DImpl( FE_TYPE const & input, std::function< void( FE_TYPE const & ) > & lambda );

template< typename FE_TYPE >
void
dispatch3DImpl( FE_TYPE & input, std::function< void( FE_TYPE & ) > & lambda );

template< typename FE_TYPE >
void
dispatch2DImpl( FE_TYPE const & input, std::function< void( FE_TYPE const & ) > & lambda );


template< typename LAMBDA > 
void
dispatch3D( FiniteElementBase const & input,
            LAMBDA && lambda )
{

  IF_WRAPPER_CONST( H1_Hexahedron_Lagrange1_GaussLegendre2 )
  else IF_WRAPPER_CONST( H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG1 )
  else IF_WRAPPER_CONST( H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG2 )
  else IF_WRAPPER_CONST( H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG3 )
  else IF_WRAPPER_CONST( H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG4 )
  else IF_WRAPPER_CONST( H1_Wedge_Lagrange1_Gauss6 )
  else IF_WRAPPER_CONST( H1_Tetrahedron_Lagrange1_Gauss1 )
  else IF_WRAPPER_CONST( H1_Pyramid_Lagrange1_Gauss5 )
#ifdef GEOSX_DISPATCH_VEM
  else IF_WRAPPER_CONST( H1_Tetrahedron_VEM_Gauss1 )
  else IF_WRAPPER_CONST( H1_Wedge_VEM_Gauss1 )
  else IF_WRAPPER_CONST( H1_Hexahedron_VEM_Gauss1 )
  else IF_WRAPPER_CONST( H1_Prism5_VEM_Gauss1 )
  else IF_WRAPPER_CONST( H1_Prism6_VEM_Gauss1 )
  else IF_WRAPPER_CONST( H1_Prism7_VEM_Gauss1 )
  else IF_WRAPPER_CONST( H1_Prism8_VEM_Gauss1 )
  else IF_WRAPPER_CONST( H1_Prism9_VEM_Gauss1 )
  else IF_WRAPPER_CONST( H1_Prism10_VEM_Gauss1 )
  else IF_WRAPPER_CONST( H1_Prism11_VEM_Gauss1 )
#endif
  else IF_WRAPPER_CONST( Q3_Hexahedron_Lagrange_GaussLobatto )
  else
  {
    GEOSX_ERROR( "finiteElement::dispatch3D() is not implemented for input of "<<typeid(input).name() );
  }
}

template< typename LAMBDA >
void
dispatch3D( FiniteElementBase & input,
            LAMBDA && lambda )
{

  IF_WRAPPER( H1_Hexahedron_Lagrange1_GaussLegendre2 )
  else IF_WRAPPER( H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG1 )
  else IF_WRAPPER( H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG2 )
  else IF_WRAPPER( H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG3 )
  else IF_WRAPPER( H1_Hexahedron_Lagrange1_GaussLegendre2_DEBUG4 )
  else IF_WRAPPER( H1_Wedge_Lagrange1_Gauss6 )
  else IF_WRAPPER( H1_Tetrahedron_Lagrange1_Gauss1 )
  else IF_WRAPPER( H1_Pyramid_Lagrange1_Gauss5 )
#ifdef GEOSX_DISPATCH_VEM
  else IF_WRAPPER( H1_Tetrahedron_VEM_Gauss1 )
  else IF_WRAPPER( H1_Wedge_VEM_Gauss1 )
  else IF_WRAPPER( H1_Hexahedron_VEM_Gauss1 )
  else IF_WRAPPER( H1_Prism5_VEM_Gauss1 )
  else IF_WRAPPER( H1_Prism6_VEM_Gauss1 )
  else IF_WRAPPER( H1_Prism7_VEM_Gauss1 )
  else IF_WRAPPER( H1_Prism8_VEM_Gauss1 )
  else IF_WRAPPER( H1_Prism9_VEM_Gauss1 )
  else IF_WRAPPER( H1_Prism10_VEM_Gauss1 )
  else IF_WRAPPER( H1_Prism11_VEM_Gauss1 )
#endif
  else IF_WRAPPER( Q3_Hexahedron_Lagrange_GaussLobatto )
  else
  {
    GEOSX_ERROR( "finiteElement::dispatch3D() is not implemented for input of "<<LvArray::system::demangleType( &input ) );
  }
}

template< typename LAMBDA > 
void
dispatch2D( FiniteElementBase const & input,
            LAMBDA && lambda )
{
  IF_WRAPPER_CONST_2D( H1_QuadrilateralFace_Lagrange1_GaussLegendre2 )
  else IF_WRAPPER_CONST_2D( H1_TriangleFace_Lagrange1_Gauss1 )
  else
  {
    GEOSX_ERROR( "finiteElement::dispatch2D() is not implemented for input of: "<<LvArray::system::demangleType( &input ) );
  }
}

}
}

#endif /* GEOSX_FINITEELEMENT_FINITEELEMENTDISPATCH_HPP_ */

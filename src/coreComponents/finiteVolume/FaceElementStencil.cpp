/*
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Copyright (c) 2019, Lawrence Livermore National Security, LLC.
 *
 * Produced at the Lawrence Livermore National Laboratory
 *
 * LLNL-CODE-746361
 *
 * All rights reserved. See COPYRIGHT for details.
 *
 * This file is part of the GEOSX Simulation Framework.
 *
 * GEOSX is a free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (as published by the
 * Free Software Foundation) version 2.1 dated February 1999.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

/**
 * @file FaceElementStencil.cpp
 */

#include "FaceElementStencil.hpp"

namespace geosx
{

FaceElementStencil::FaceElementStencil():
  StencilBase<FaceElementStencil_Traits,FaceElementStencil>()
{}


void FaceElementStencil::add( localIndex const numPts,
                              localIndex  const * const elementRegionIndices,
                              localIndex  const * const elementSubRegionIndices,
                              localIndex  const * const elementIndices,
                              real64 const * const weights,
                              real64 const * const weightedElementCenterToConnectorCenterSquare,
                              localIndex const connectorIndex )
{
  GEOS_ERROR_IF( numPts >= MAX_STENCIL_SIZE, "Maximum stencil size exceeded" );

  stackArray1d<localIndex, 1> connectorIndexArray;
  connectorIndexArray[0] = connectorIndex;

  typename decltype( m_stencilIndices )::iterator iter = m_stencilIndices.find(connectorIndex);
  if( iter==m_stencilIndices.end() )
  {
    m_elementRegionIndices.appendArray( elementRegionIndices, numPts );
    m_elementSubRegionIndices.appendArray( elementSubRegionIndices, numPts );
    m_elementIndices.appendArray( elementIndices, numPts );
    m_weights.appendArray( weights, numPts );
    m_weightedElementCenterToConnectorCenterSquare.appendArray( weightedElementCenterToConnectorCenterSquare, numPts );

    m_stencilIndices[connectorIndex] = m_weights.size() - 1;
  }
  else
  {
    localIndex const stencilIndex = iter->second;
    m_elementRegionIndices.clearArray( stencilIndex );
    m_elementSubRegionIndices.clearArray( stencilIndex );
    m_elementIndices.clearArray( stencilIndex );
    m_weights.clearArray( stencilIndex );
    m_weightedElementCenterToConnectorCenterSquare.clearArray( stencilIndex );

    m_elementRegionIndices.appendToArray( stencilIndex, elementRegionIndices, numPts );
    m_elementSubRegionIndices.appendToArray( stencilIndex, elementSubRegionIndices, numPts );
    m_elementIndices.appendToArray( stencilIndex, elementIndices, numPts );
    m_weights.appendToArray( stencilIndex, weights, numPts );
    m_weightedElementCenterToConnectorCenterSquare.appendToArray( stencilIndex, weightedElementCenterToConnectorCenterSquare, numPts );
  }
}


} /* namespace geosx */

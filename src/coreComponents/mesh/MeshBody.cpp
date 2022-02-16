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
 * @file MeshBody.cpp
 */

#include "MeshBody.hpp"
#include "MeshLevel.hpp"
#include "generators/CellBlockManager.hpp"

namespace geosx
{
using namespace dataRepository;

MeshBody::MeshBody( string const & name,
                    Group * const parent ):
  Group( name, parent ),
  m_meshLevels( registerGroup( groupStructKeys::meshLevelsString() ) ),
  m_globalLengthScale( 0 )
{
  m_hasParticles = false; // By default we assume no particles in a mesh body... SJP
}

MeshBody::~MeshBody()
{
  // TODO Auto-generated destructor stub
}



MeshLevel & MeshBody::createMeshLevel( localIndex const newLevel )
{
  return m_meshLevels.registerGroup< MeshLevel >( intToMeshLevelString( newLevel ) );
}

void MeshBody::setGlobalLengthScale( real64 scale )
{
  m_globalLengthScale = scale;
}

string MeshBody::intToMeshLevelString( localIndex const meshLevel )
{
  char temp[100] = {0};
  sprintf( temp, "Level%.1ld", meshLevel );
  return temp;
}


} /* namespace geosx */

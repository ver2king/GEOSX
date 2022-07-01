/*	
 * ------------------------------------------------------------------------------------------------------------	
 * SPDX-License-Identifier: LGPL-2.1-only	
 *	
 * Copyright (c) 2018-2020 Lawrence Livermore National Security LLC	
 * Copyright (c) 2018-2020 The Board of Trustees of the Leland Stanford Junior University	
 * Copyright (c) 2018-2020 Total, S.A	
 * Copyright (c) 2020-     GEOSX Contributors	
 * All right reserved	
 *	
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.	
 * ------------------------------------------------------------------------------------------------------------	
 */

#include "FaceBlock.hpp"

namespace geosx {

localIndex FaceBlock::numFaces() const
{
  return m_numFaces;
}

ArrayOfArrays< localIndex > FaceBlock::getFaceToNodes() const
{
  return m_faceToNodes;
}

ArrayOfArrays< localIndex > FaceBlock::getFaceToEdges() const
{
  return m_faceToEdges;
}

ToCellRelation< array2d< localIndex > > FaceBlock::getFaceToElements() const
{
  return m_faceToElements;
}

}
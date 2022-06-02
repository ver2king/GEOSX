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

#ifndef GEOSX_FACEBLOCK_HPP
#define GEOSX_FACEBLOCK_HPP

#include "dataRepository/Group.hpp"
#include "common/DataTypes.hpp"

#include "FaceBlockABC.hpp"

namespace geosx
{

class FaceBlock : public FaceBlockABC
{
public:
  FaceBlock( string const & name,
             Group * const parent )
    :
    FaceBlockABC( name, parent )
  { }

  localIndex numFaces() const override;

  ArrayOfArrays< localIndex > getFaceToNodes() const override;

  ArrayOfArrays< localIndex > getFaceToEdges() const override;

  ToCellRelation< array2d< localIndex > > getFaceToElements() const override;
};

} // end of namespace geosx

#endif //GEOSX_FACEBLOCK_HPP

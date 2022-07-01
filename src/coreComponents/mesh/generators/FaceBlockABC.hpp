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

#ifndef GEOSX_FACEBLOCKABC_HPP
#define GEOSX_FACEBLOCKABC_HPP

#include "ToCellRelation.hpp"

#include "dataRepository/Group.hpp"
#include "common/DataTypes.hpp"


namespace geosx
{

class FaceBlockABC : public dataRepository::Group
{
public:
  FaceBlockABC( string const & name,
                Group * const parent )
    :
    Group( name, parent )
  { }

  virtual localIndex numFaces() const = 0;

  virtual ArrayOfArrays< localIndex > getFaceToNodes() const = 0; // TODO getFaceToNodesLeft, getFaceToNodesRight

  virtual ArrayOfArrays< localIndex > getFaceToEdges() const = 0;

  virtual ToCellRelation< array2d< localIndex > > getFaceToElements() const = 0; // TODO and get face to face index of element?
};

} // end of namespace geosx

#endif //GEOSX_FACEBLOCKABC_HPP

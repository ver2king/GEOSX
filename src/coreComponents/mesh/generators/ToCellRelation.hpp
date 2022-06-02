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

#ifndef GEOSX_TOCELLRELATION_HPP
#define GEOSX_TOCELLRELATION_HPP

namespace geosx
{

/**
 * @brief Container for maps from a mesh object (node, edge or face) to cells.
 * @tparam BASETYPE underlying map type
 */
template< typename BASETYPE >
struct ToCellRelation
{
  BASETYPE toBlockIndex; ///< Map containing a list of cell block indices for each object
  BASETYPE toCellIndex;  ///< Map containing cell indices, same shape as above
};

}

#endif //GEOSX_TOCELLRELATION_HPP

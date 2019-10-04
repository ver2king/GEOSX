/*
 * ------------------------------------------------------------------------------------------------------------
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Copyright (c) 2018-2019 Lawrence Livermore National Security LLC
 * Copyright (c) 2018-2019 The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2018-2019 Total, S.A
 * Copyright (c) 2019-     GEOSX Contributors
 * All right reserved
 *
 * See top level LICENSE, COPYRIGHT, CONTRIBUTORS, NOTICE, and ACKNOWLEDGEMENTS files for details.
 * ------------------------------------------------------------------------------------------------------------
 */

#ifndef GEOSX_FINITEELEMENT_QUADRATURERULEMANAGER_HPP_
#define GEOSX_FINITEELEMENT_QUADRATURERULEMANAGER_HPP_

#include "dataRepository/Group.hpp"
#include "dataRepository/xmlWrapper.hpp"

namespace geosx
{
namespace dataRepository
{
namespace keys
{
}
}


class QuadratureRuleManager : public dataRepository::Group
{
public:
  QuadratureRuleManager() = delete;
  QuadratureRuleManager(string const & name, Group * const parent);
  virtual ~QuadratureRuleManager() override;

  virtual Group * CreateChild( string const & childKey, string const & childName ) override;

  /// This function is used to expand any catalogs in the data structure
  virtual void ExpandObjectCatalogs() override;

};

} /* namespace geosx */

#endif /* GEOSX_FINITEELEMENT_QUADRATURERULEMANAGER_HPP_ */

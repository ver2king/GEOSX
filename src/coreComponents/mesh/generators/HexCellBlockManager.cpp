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

#include "HexCellBlockManager.hpp"

#include <algorithm>
#include <numeric>
#include <cassert>


namespace geosx
{
  using vertexIndex = localIndex;
  using cellIndex = localIndex;
  using faceIndex = localIndex;
  using edgeIndex = localIndex;
  using cellBlockIndex = localIndex;

  using cellFaceIndex = localIndex; // A  face in a cell nbFaces * idCell + f
  using cellEdgeIndex = localIndex; // An edge in a cell numEdges * idCell + e
  using SizeOfStuff = localIndex;

  using cellVertexIndices = array2d<localIndex, cells::NODE_MAP_PERMUTATION>;

  // Use explicit aliases for indices in a hexahedron
  using hexVertexIndex = unsigned int; // a vertex in a hex 0 to 8
  using hexEdgeIndex = unsigned int;   // a edge in a hex 0 to 12
  using hexFacetIndex = unsigned int;  // a facet in a hex 0 to 6

  static const int NO_ID = -1;

  /*  Hexahedron template
   *  WARNING - Hex vertex numbering in GEOSX differs from the one used
   *  by most mesh datastructures - There are further variations in GEOSX itself
   *
   *   6----------7
   *   |\         |\
   *   | \        | \
   *   |  \       |  \
   *   |   4------+---5
   *   |   |      |   |
   *   2---+------3   |
   *    \  |       \  |
   *     \ |        \ |
   *      \|         \|
   *       0----------1
   */
  struct Hex
  {
    static constexpr unsigned int numVertices = 8;
    static constexpr unsigned int numEdges = 12;
    static constexpr unsigned int numFacets = 6;
    static constexpr unsigned int numEdgesPerFacet = 4;
    static constexpr unsigned int numNodesPerFacet = 4;

    static constexpr hexVertexIndex facetVertex[numFacets][numNodesPerFacet] = {
        {0, 1, 3, 2}, {4, 5, 7, 6}, {0, 1, 5, 4}, {1, 3, 7, 5}, {2, 3, 7, 6}, {0, 2, 6, 4}};
    //         0             1           2            3             4               5
    static constexpr hexVertexIndex edgeVertex[numEdges][2]{
        {0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3}, {2, 6}, {3, 7}, {4, 5}, {4, 6}, {5, 7}, {6, 7}};

    static constexpr hexVertexIndex edgeAdjacentFacet[numEdges][2]{
        {0, 2}, {0, 5}, {2, 5}, {0, 3}, {2, 3}, {0, 4}, {5, 4}, {3, 4}, {2, 1}, {1, 5}, {1, 3}, {1, 4}};

    // This the ordering needed to compute consistent normals
    // static constexpr hexVertexIndex orientedFacetVertex[6][4] = {
    //    {0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1}, {1, 5, 7, 3}, {2, 3, 7, 6}, {0, 2, 6, 4}};
  };


  /* Structure used by MeshConnectivityBuilder to compute faces
   * 
   * TODO Optimize? replace (v0, v1, v2) by (v0*m_numNodes + v1, v2)
   */
  struct FaceInfo
  {
    /* 
     * Compare the indices of the 3 sorted vertex indices
     */
    bool operator<(FaceInfo const &right) const
    {
      if (v[0] != right.v[0]) return v[0] < right.v[0];
      if (v[1] != right.v[1]) return v[1] < right.v[1];
      if (v[2] != right.v[2]) return v[2] < right.v[2];
      return false;
    }

    /// A face is identified by its 3 smalled indices v0 < v1 < v2
    vertexIndex v[3];
    /// Unambiguous identification of the cell facet of the mesh it comes from
    cellFaceIndex cellFace;  //  Hex::numFacets * idCell + hexFacetIndex
  };

  /* 
   * Structure used by MeshConnectivityBuilder for edge computation and storage
   */
  struct EdgeInfo
  {
    EdgeInfo(vertexIndex a, cellEdgeIndex b)
    {
      first = a;
      second = b;
    }
    EdgeInfo() : EdgeInfo(0, 0) {}

    /* Two edges are the same if their vertices are the same */
    bool operator==(EdgeInfo const &right) const
    {
      return first == right.first;
    }

    /* Two edges are compared relatively to their vertex indices only */
    bool operator<(EdgeInfo const &right) const
    {
      return first < right.first;
    }

    /// The two nodes v0-v1, with v0 < v1, stored as first v0 * m_numNodes +v1
    vertexIndex first;
    /// Unambiguous identification of the cell edge it comes from 
    cellEdgeIndex second;   // Hex::numEdges * idCell + hexEdgeIndex
  };

  /***************************************************************************************/
  /***************************************************************************************/

  /* Debugging functionalites */

  void print(std::vector<localIndex> const &in)
  {
    int count = 0;
    for (unsigned int i = 0; i < in.size(); ++i)
    {
      if (count == 10)
      {
        std::cout << std::endl;
        count = 0;
      }
      std::cout << std::setw(5) << std::left << in[i];
      count++;
    }
    std::cout << std::endl;
  }
  void print(std::vector<bool> const &in)
  {
    int count = 0;
    for (unsigned int i = 0; i < in.size(); ++i)
    {
      if (count == 10)
      {
        std::cout << std::endl;
        count = 0;
      }
      std::cout << std::setw(5) << std::left << in[i];
      count++;
    }
    std::cout << std::endl
              << std::endl;
  }

  void print(ArrayOfSets<localIndex> const &in)
  {
    for (unsigned int i = 0; i < in.size(); ++i)
    {
      for (unsigned int j = 0; j < in.sizeOfSet(i); ++j)
      {
        std::cout << std::setw(5) << std::left << in(i, j);
      }
      std::cout << std::endl;
    }
  }

  void print(array2d<localIndex> const &in)
  {
    for (unsigned int i = 0; i < in.size(0); ++i)
    {
      std::cout << std::setw(5) << std::left << in(i,0);
      std::cout << std::setw(5) << std::left << in(i,1);
  }
  std::cout << std::endl
            << std::endl;
}
void print(std::vector<EdgeInfo> const &in)
{
  for (unsigned int  i = 0; i < in.size(); ++i)
  {
    std::cout << std::setw(5) << i
              << std::setw(5) << std::left << in[i].first 
              << std::setw(5) << std::left << in[i].second 
              << std::endl;
  }
  std::cout << std::endl
            << std::endl;
}

/***************************************************************************************/
/***************************************************************************************/


/**
 * @class MeshConnectivityBuilder
 * @brief The MeshConnectivityBuilder to build the connectivity maps 
 * 
 * Initially designed for hexahedral meshes (unstructured)
 * TODO How do we reuse this for other type of cells 
 * Most of the code will be the same 
 * Is templating an option? Or derivation? 
 * 
 * TODO and here is ONE problem: all mapping is toward Elements are not safe 
 * since the elements may not be in the same CellBlock
 * Check what was done - Where is this used and what for?
 *
 * TODO Why are storage strategies different for the mappings ?
 * TODO Why multidimensional arrays? Isn't is more expensive? 
 * TODO Implement specialization for regular hex mesh
 * 
 * TODO The storage of Faces and Edges is dependant on the Cell Types  
 * How do we manage the CellBlocks with different types of cells?
 * Strategy 1: allocate the space for hexahedra and keep a lot of invalid stuff 
 *             in these vectors - not the worst idea since these meshes should be hex-dominant
 *             do we need to store the cell type?
 * TODO For full tetrahedral meshes we need a dedicated implementation
 * 
 * Options : Store if the face is a triangle or quad ? 
 *  Store a  NO_ID  value if  same storage space for hybrid meshes
 *  
 */
class MeshConnectivityBuilder
{
public:
  MeshConnectivityBuilder(CellBlockManagerBase & cellBlockManager);
  MeshConnectivityBuilder( const MeshConnectivityBuilder & ) = delete;
  MeshConnectivityBuilder & operator=( const MeshConnectivityBuilder & ) = delete;
  virtual ~MeshConnectivityBuilder() = default;

  localIndex numEdges() const {
    return m_uniqueEdges.size();
  }
  localIndex numFaces() const {
    return m_uniqueFaces.size();
  }

  // Dependent on Cell Type
  virtual void computeFaces() = 0;
  virtual void computeEdges() = 0;

  // Independent on Cell Type
  void computeNodesToEdges( ArrayOfSets<localIndex> & result ) const; 
  void computeEdgesToNodes( array2d<localIndex>& result ) const;  
  void computeNodesToElements( ArrayOfArrays<localIndex>  & result ) const;

  // Dependent on Cell Type
  virtual void computeNodesToFaces( ArrayOfSets<localIndex> & result ) const = 0 ;
  virtual void computeEdgesToFaces( ArrayOfSets<localIndex> & result ) const  = 0;
  virtual void computeFacesToNodes( ArrayOfArrays<localIndex> & result ) const = 0;
  virtual void computeFacesToElements( array2d<localIndex> &  result ) const  = 0;
  virtual void computeFacesToEdges( ArrayOfArrays<localIndex> & result ) const =0 ;

  // Fill in the mappings stored by the CellBlocks
  virtual void computeElementsToFacesOfCellBlocks() = 0; 
  virtual void computeElementsToEdgesOfCellBlocks() = 0;

  void debuggingComputeAllMaps() const;

  void printDebugInformation() const;

protected:
  std::pair< cellBlockIndex, cellIndex> 
  getBlockCellFromManagerCell( cellIndex cellId ) const;

  unsigned int numCellBlocks() const {
    return m_cellBlocks.size();
  } 
  CellBlock const & getCellBlock( unsigned int id ) const {
    return *(m_cellBlocks[id]);
  }
  CellBlock & getCellBlock( unsigned int id ){
    return *(m_cellBlocks[id]);
  }
 
  std::vector< localIndex > computeAllFacesToUniqueFace() const;

  virtual void getOneEdgeToFaces( int edgeIndex,
                           std::vector< localIndex > const & allFacesToUniqueFace, 
                           std::set <localIndex> & faces ) const = 0;

protected:
/// Number of vertices 
SizeOfStuff m_numNodes = 0;
/// All Elements in all CellBlocks
SizeOfStuff m_numElements = 0;  

/// Cell Blocks on which the class operates - Size of nbBlocks
std::vector< CellBlock * > m_cellBlocks;

/// Offset for the numbering of all the cells - First value is the number of cells of block 0
/// Size of nbBlocks
std::vector< cellIndex > m_blockCellIndexOffset;

/* Storage of a minimal set of information to iterate through
 * the faces while storing to which face of which cell they belong and which
 * is the neighbor face is the neighbor cell.
 * Use the numbering of cells managed by this class max is m_numElements
 * Each face of each cell is encoded by 6 * cellIndex + faceIndexInCell
 * 
 * TODO Implement for tetrahedra (6 becomes 4)
 * TODO Define the strategy for hybrid FE meshes (hex, prism, pyramids, tets)
 */
std::vector< cellFaceIndex > m_allFacesToNeighbors;  // 6 * m_numElements
std::vector< localIndex > m_uniqueFaces;              // nbFaces
std::vector< bool > m_isBoundaryFace;                 // nbFaces

/* Storage of a minimal set of information to iterate through
 * the edges faces while storing to which face of which cell they belong to
 */
std::vector< EdgeInfo > m_allEdges;                 // 12 * m_numElements
std::vector< localIndex > m_uniqueEdges;            // numEdges

};

class HexMeshConnectivityBuilder : public MeshConnectivityBuilder
{
public:
  HexMeshConnectivityBuilder(CellBlockManagerBase & cellBlockManager):
    MeshConnectivityBuilder(cellBlockManager){};
  HexMeshConnectivityBuilder( const HexMeshConnectivityBuilder & ) = delete;
  HexMeshConnectivityBuilder & operator=( const HexMeshConnectivityBuilder & ) = delete;
  ~HexMeshConnectivityBuilder() = default;

  void computeFaces() override;
  void computeEdges() override;
  
  virtual void computeElementsToFacesOfCellBlocks() override;
  virtual void computeElementsToEdgesOfCellBlocks() override;

  void computeNodesToFaces(ArrayOfSets<localIndex> &result) const override;
  void computeEdgesToFaces(ArrayOfSets<localIndex> &result) const override;
  void computeFacesToNodes(ArrayOfArrays<localIndex> &result) const override;
  void computeFacesToElements(array2d<localIndex> &result) const override;
  void computeFacesToEdges(ArrayOfArrays<localIndex> &result) const override;

protected:
  void getOneEdgeToFaces(int edgeIndex,
                          std::vector<localIndex> const &allFacesToUniqueFace,
                          std::set<localIndex> &faces) const override;
};



MeshConnectivityBuilder::MeshConnectivityBuilder( CellBlockManagerBase & cellBlockManager )
{
  m_numNodes = cellBlockManager.numNodes();
  
  dataRepository::Group & group = cellBlockManager.getCellBlocks();
  localIndex nbBlocks = group.numSubGroups();
  
  m_cellBlocks.resize(nbBlocks);
  m_blockCellIndexOffset.resize(nbBlocks);

  for (int i = 0; i < nbBlocks; ++i)
  {
    m_cellBlocks[i] = &group.getGroup< CellBlock >( i );
    m_blockCellIndexOffset[i] = m_cellBlocks[i]->numElements();
  }
  std::partial_sum( m_blockCellIndexOffset.begin(), m_blockCellIndexOffset.end(), m_blockCellIndexOffset.begin() );
  m_numElements =  m_blockCellIndexOffset.back();

  GEOSX_ERROR_IF( nbBlocks <= 0, " Invalid number of CellBlocks in mesh connectivity computation");
}

// TODO convert to log output
void MeshConnectivityBuilder::printDebugInformation() const
{ 
   std::cout << std::endl
            << std::endl;
  
  std::cout << " Number of blocks : " << m_cellBlocks.size() << std::endl
            << std::endl;
  print(m_blockCellIndexOffset);  

  std::cout << std::endl
            << std::endl;

  std::cout << " Total number of elements : " << m_numElements << std::endl
            << std::endl;


  std::cout << " Number of unique faces : " << numFaces() << std::endl
            << std::endl;

  std::cout << " Face Info " << std::endl
            << std::endl;

  print(m_allFacesToNeighbors);
  std::cout << std::endl
            << std::endl;

  print(m_uniqueFaces);
  std::cout << std::endl
            << std::endl;

  print(m_isBoundaryFace);
  std::cout << std::endl
            << std::endl;

  std::cout << " NB Edges : " << numEdges() << std::endl
            << std::endl;

  std::cout << " Edge Info " << std::endl
            << std::endl;

  print(m_allEdges);
  std::cout << std::endl
          << std::endl;
  
  print(m_uniqueEdges);
  std::cout << std::endl
            << std::endl;

  return;
}

std::pair< cellBlockIndex, cellIndex> 
MeshConnectivityBuilder::getBlockCellFromManagerCell( cellIndex cellId ) const
{
  cellBlockIndex b = 0;
  while( b < numCellBlocks() && cellId > m_blockCellIndexOffset[b] )
  {
    b++;
  }
  localIndex offset = b > 0 ? m_blockCellIndexOffset[b-1] : 0;
  cellIndex c = cellId - offset;

  return std::pair< cellBlockIndex, cellIndex> (b, c);
}


std::vector< localIndex > MeshConnectivityBuilder::computeAllFacesToUniqueFace() const
{
  std::vector< localIndex > allFacesToUniqueFace ( m_allFacesToNeighbors.size(), NO_ID );

  for( unsigned int curFace = 0; curFace < m_uniqueFaces.size(); ++curFace)
  {
    localIndex f = m_uniqueFaces[curFace];
    allFacesToUniqueFace[f] = curFace;
    if( !m_isBoundaryFace[curFace] )
    {
      localIndex f1 = m_allFacesToNeighbors[f];
      allFacesToUniqueFace[f1] = curFace;
    }
  }
  return allFacesToUniqueFace;  
}

// Cell type independent
void MeshConnectivityBuilder::computeEdgesToNodes ( array2d<localIndex> & edgeToNodes ) const
{
  edgeToNodes.resize(numEdges(), 2);
  // Initialize contents
  edgeToNodes.setValues< serialPolicy >( NO_ID );
 
  for (unsigned int i = 0; i < m_uniqueEdges.size(); ++i)
  {
    EdgeInfo e = m_allEdges[m_uniqueEdges[i]];
    vertexIndex v0 = e.first / m_numNodes;
    vertexIndex v1 = e.first % m_numNodes;

    edgeToNodes[i][0] = v0;
    edgeToNodes[i][1] = v1;
  }
  
}

// Cell type independent - There is no need for this to be an ArrayOfSets
// We fill it with unique (maybe sorted) values
void MeshConnectivityBuilder::computeNodesToEdges( ArrayOfSets<localIndex> & nodeToEdges ) const
{ 
  // 1 - Counting 
  // TODO Can be skipped for hexahedral meshes - 6 for regular nodes - 10 tops for singular nodes
  std::vector<unsigned int> nbEdgesPerNode(m_numNodes, 0);
  for( unsigned int i = 0; i < m_uniqueEdges.size(); ++i)
  {
    EdgeInfo e = m_allEdges[ m_uniqueEdges[i] ];
    vertexIndex v0 = e.first / m_numNodes;
    vertexIndex v1 = e.first % m_numNodes;
    nbEdgesPerNode[v0]++;
    nbEdgesPerNode[v1]++;
  }
  localIndex valuesToReserve = std::accumulate(nbEdgesPerNode.begin(), nbEdgesPerNode.end(), 0);

  // 2 - Allocating 
  nodeToEdges.resize( 0 );
  nodeToEdges.reserve( m_numNodes );
  nodeToEdges.reserveValues( valuesToReserve );

  // Append and set the capacity of the individual arrays.
  for (localIndex n = 0; n < m_numNodes; ++n)
  {
    nodeToEdges.appendSet( nbEdgesPerNode[n] );
  }
  
  // 3 - Filling
  for(unsigned  int i = 0; i < m_uniqueEdges.size(); ++i)
  {
    EdgeInfo e = m_allEdges[ m_uniqueEdges[i] ];
    vertexIndex v0 = e.first / m_numNodes;
    vertexIndex v1 = e.first % m_numNodes;

    nodeToEdges.insertIntoSet( v0, i); 
    nodeToEdges.insertIntoSet( v1, i);
  }
  
}

// Cell type independent
void MeshConnectivityBuilder::computeNodesToElements( ArrayOfArrays<localIndex> & nodeToElements ) const
{
  // 1 -  Counting
  // TODO Can be skipped for hexahedral meshes - 8 for regular nodes - 12 tops for singular nodes
  std::vector<unsigned int> nbElementsPerNode(m_numNodes, 0);
  for (unsigned int i = 0; i < numCellBlocks(); ++i)
  {
    CellBlock const & block = getCellBlock(i);
    cellVertexIndices const & cells = block.getElemToNodes();
    localIndex numVertices = block.numNodesPerElement();

    for (int j = 0; j < block.numElements(); ++j)
    {
      for (unsigned int v = 0; v < numVertices; ++v)
      {
        nbElementsPerNode[ cells( j, v ) ]++;   
      }
    }
  }
  localIndex nbValues = std::accumulate(nbElementsPerNode.begin(), nbElementsPerNode.end(), 0);

  //  2 - Allocating - No overallocation
  nodeToElements.resize(0);
  nodeToElements.resize(m_numNodes);
  nodeToElements.reserveValues( nbValues ); // Does this accelerate allocation?

  for( int i = 0; i < m_numNodes; ++i )
  {
    nodeToElements.setCapacityOfArray(i, nbElementsPerNode[i]);
  }

  // 3 - Set the values
  for (unsigned int i = 0; i < numCellBlocks(); ++i)
  {
    CellBlock const & block = getCellBlock(i);
    cellVertexIndices const & cells = block.getElemToNodes();
    localIndex numVertices = block.numNodesPerElement();

    for (int j = 0; j < block.numElements(); ++j)
    {
      for (unsigned int v = 0; v < numVertices; ++v)
      {
        localIndex const nodeIndex = cells(j, v);
        nodeToElements.emplaceBack(nodeIndex, j);
      }
    }
  }
  
}


/**
 * @brief Compute all the faces of the cells of the cell blocks
 * Fills the face information 
 * TODO Be able to get out and return an error message if 3 cells have the same face
 * 
 * TODO CellType dependant 
 * TODO What management for triangles? Fill with NO_VERTEX the 4th vertex and 
 * have consistent cell descriptions?
 * The block knows the cell type, we could use this? 
 * Maybe sort CellBlocks by cell type, and template things
 */
void HexMeshConnectivityBuilder::computeFaces()
{
  m_allFacesToNeighbors.resize( 0);
  m_uniqueFaces.resize( 0 ); 
  m_isBoundaryFace.resize( 0 ); 
  
  // 1 - Allocate - 
  SizeOfStuff nbTotalFaces = m_numElements * Hex::numFacets;
  m_allFacesToNeighbors.resize( nbTotalFaces);
  
  // To collect and sort the facets 
  std::vector< FaceInfo > allFaces ( nbTotalFaces );

  // 2 - Fill 
  localIndex curFace = 0;
  for (unsigned int i = 0; i < numCellBlocks(); ++i)
  {
    CellBlock const & block = getCellBlock(i);
    cellVertexIndices const & cells = block.getElemToNodes();

    for (int j = 0; j < cells.size(0); ++j)
    {
      for (unsigned int f = 0; f < Hex::numFacets; ++f)
      {
        vertexIndex v0 = cells( j, Hex::facetVertex[f][0] );
        vertexIndex v1 = cells( j, Hex::facetVertex[f][1] );
        vertexIndex v2 = cells( j, Hex::facetVertex[f][2] );
        vertexIndex v3 = cells( j, Hex::facetVertex[f][3] );

        // Sort the vertices 
        if( v0 > v1 ) std::swap( v0, v1 );
        if( v2 > v3 ) std::swap( v2, v3 );
        if( v0 > v2 ) std::swap( v0, v2 );
        if( v1 > v3 ) std::swap( v1, v3 );
        if( v1 > v2 ) std::swap( v1, v2 );

        allFaces[curFace].v[0] = v0;
        allFaces[curFace].v[1] = v1;
        allFaces[curFace].v[2] = v2;
        // If the mesh is valid, then if 2 quad faces share 3 vertices they are the same
        // Last slot is used to identify the facet in its cell 
        allFaces[curFace].cellFace= Hex::numFacets * j + f; 
        
        curFace++;
      }
    }
  }

  // 3 - Sort 
  // TODO Definitely not the fastest we can do - Use of HXTSort if possible? Has LvArray faster sort? 
  std::sort( allFaces.begin(), allFaces.end() );
  
  // That is an overallocation, about twice as big as needed
  m_uniqueFaces.resize( allFaces.size() ); 
  m_isBoundaryFace.resize( allFaces.size(), false ); 

  // 4 - Counting + Set Cell Adjacencies
  curFace = 0;
  int i = 0;
  for( ; i+1 < nbTotalFaces; ++i )
  {
    FaceInfo f = allFaces[i];
    FaceInfo f1 = allFaces[i+1];

    // If two successive faces are the same - this is an interior facet
    if( f.v[0]==f1.v[0] && f.v[1]==f1.v[1] && f.v[2]==f1.v[2] )
    {
      m_allFacesToNeighbors[f.cellFace] = f1.cellFace;
      m_allFacesToNeighbors[f1.cellFace] = f.cellFace;
      m_uniqueFaces[curFace] = f.cellFace;
      ++i;
    }
    // If not this is a boundary face
    else
    {
      m_allFacesToNeighbors[f.cellFace] = NO_ID;
      m_uniqueFaces[curFace] = f.cellFace;
      m_isBoundaryFace[curFace] = true;
    }
    curFace++;
  }
  // Last facet is a boundary facet 
  if (i < nbTotalFaces) 
  {
    FaceInfo f = allFaces[i];
    m_allFacesToNeighbors[f.cellFace] = NO_ID;
    m_uniqueFaces[curFace] = allFaces[i].cellFace;
    m_isBoundaryFace[curFace] = true;
    curFace++;
  }
  // Set the number of faces 
  SizeOfStuff nbFaces = curFace; 
  // Remove the non-filled values 
  m_uniqueFaces.resize( nbFaces );
  m_isBoundaryFace.resize( nbFaces );

  
}

/**
 * @brief Compute all the edges of the cells of the cell blocks
 * Fills the edge information 
 * 
 * TODO CellType dependant 
 */
void HexMeshConnectivityBuilder::computeEdges()
{
  m_allEdges.resize( 0 );
  m_uniqueEdges.resize( 0 );

  // 1 - Allocate 
  SizeOfStuff nbAllEdges = m_numElements * Hex::numEdges;
  m_allEdges.resize( nbAllEdges );
  
  // 2 - Get all edges
  localIndex cur = 0 ;
  for (unsigned int i = 0; i < numCellBlocks(); ++i)
  {
    CellBlock const & block = getCellBlock(i);
    cellVertexIndices const & cells = block.getElemToNodes();

    for (int j = 0; j < cells.size(0); ++j)
    {
      for(unsigned int e = 0; e < Hex::numEdges; ++e)
      {
        GEOSX_ERROR_IF (cur >= nbAllEdges, "Invalid edge computation" );

        vertexIndex v0 = cells( j, Hex::edgeVertex[e][0]);
        vertexIndex v1 = cells( j, Hex::edgeVertex[e][1]);

        if (v1 < v0 ) { std::swap(v0, v1); }
        vertexIndex v = v0 * m_numNodes + v1;
        cellEdgeIndex id = j * Hex::numEdges + e;

        m_allEdges[cur] = EdgeInfo(v, id);
        ++cur;
      }
    }
  }
  // 3 - Sort according to vertices 
  std::sort( m_allEdges.begin(), m_allEdges.end() );
  
  // 4 - Reserve space unique edges 
  // If a CellBlockManager manages a connected set of cells (any type of cell)
  // bounded by a sphere m_numNodes - nbEdges + nbFaces - m_numElements = 1 
  // For Hexes: 12 * nbCells = 4 * nbEdges + BoundaryEdges + Singularities - speculation 3.5 factor
  SizeOfStuff guessNbEdges = numFaces() != 0 ? m_numNodes + numFaces() - m_numElements : 3.5 * m_numElements;
  m_uniqueEdges.reserve( guessNbEdges );

  // 4 - Get unique edges
  unsigned int i = 0;
  while( i < m_allEdges.size() )
  {
    m_uniqueEdges.push_back(i);
    unsigned int j = i+1;
    while( j < m_allEdges.size() && (m_allEdges[i] < m_allEdges[j]) )
    { 
      ++j; 
    }
    i = j;
  }
  
}


void HexMeshConnectivityBuilder::computeFacesToNodes( ArrayOfArrays<localIndex> & faceToNodes ) const
{
  // 1 - Allocate - No overallocation
  faceToNodes.resize(0);
  faceToNodes.resize( numFaces(), Hex::numNodesPerFacet );

  // TODO Check if this resize or reserve the space for the inner arrays
  // In doubt resize inner arrays
  for (int i = 0; i < numFaces(); ++i)
  {
    faceToNodes.resizeArray(i, Hex::numNodesPerFacet);
  }

  // 2 - Fill FaceToNode  -- Could be avoided and done when required from adjacencies
  for( unsigned int curFace = 0; curFace < m_uniqueFaces.size(); ++curFace)
  {
    localIndex f = m_uniqueFaces[curFace];
    cellIndex c = f / Hex::numFacets;

    // We want the nodes of face f % 6 in cell c 
    auto blockCell = getBlockCellFromManagerCell(c) ;
    localIndex blockIndex = blockCell.first;
    localIndex cellInBlock = blockCell.second;

    CellBlock const & cB = getCellBlock( blockIndex );
    hexFacetIndex faceToStore = f % Hex::numFacets;

    GEOSX_ERROR_IF(cellInBlock >= 0, "Unexpected error in mesh mapping computations");  

    // Maybe the oriented facets would be useful - if they are the ones recomputed 
    // later one by the FaceManager
    faceToNodes[curFace][0] = cB.getElementNode(cellInBlock, Hex::facetVertex[faceToStore][0]);
    faceToNodes[curFace][1] = cB.getElementNode(cellInBlock, Hex::facetVertex[faceToStore][1]);
    faceToNodes[curFace][2] = cB.getElementNode(cellInBlock, Hex::facetVertex[faceToStore][2]);
    faceToNodes[curFace][3] = cB.getElementNode(cellInBlock, Hex::facetVertex[faceToStore][3]);
  }
  
}


void HexMeshConnectivityBuilder::computeNodesToFaces( ArrayOfSets<localIndex> & nodeToFaces ) const
{
  ArrayOfArrays<localIndex> faceToNodes; 
  computeFacesToNodes( faceToNodes );

  // 1 - Counting  - Quite unecessary for hexahedral meshes 
  //( 12 for regular nodes  - TODO check a good max for singular node )
  std::vector<unsigned int> nbFacesPerNode(m_numNodes, 0);
  for(unsigned int i = 0; i < faceToNodes.size(); ++i)
  {
    nbFacesPerNode[faceToNodes[i][0]]++;
    nbFacesPerNode[faceToNodes[i][1]]++;
    nbFacesPerNode[faceToNodes[i][2]]++;
    nbFacesPerNode[faceToNodes[i][3]]++;
  }
  localIndex valuesToReserve = std::accumulate(nbFacesPerNode.begin(), nbFacesPerNode.end(), 0);

  // 2 - Allocating 
  nodeToFaces.resize(0);
  nodeToFaces.resize(m_numNodes);
  nodeToFaces.reserveValues( valuesToReserve );
  for (localIndex n = 0; n < m_numNodes; ++n)
  {
    nodeToFaces.appendSet( nbFacesPerNode[n] );
  }
  
  // 3 - Filling 
  for(unsigned int i = 0; i < faceToNodes.size(); ++i)
  {
    nodeToFaces.insertIntoSet( faceToNodes[i][0],i );
    nodeToFaces.insertIntoSet( faceToNodes[i][1],i );
    nodeToFaces.insertIntoSet( faceToNodes[i][2],i );
    nodeToFaces.insertIntoSet( faceToNodes[i][3],i );
  }
}


// TODO We have a problem - where are the Element index valid ?
void HexMeshConnectivityBuilder::computeFacesToElements( array2d<localIndex> & faceToElements ) const
{
  faceToElements.resize(0);
  faceToElements.resize(numFaces(), 2);
  // Initialize contents to NO_ID 
  faceToElements.setValues< serialPolicy >( NO_ID );

  for(unsigned int curFace = 0; curFace < m_uniqueFaces.size(); ++curFace)
  {
    localIndex f = m_uniqueFaces[curFace];
    cellFaceIndex neighbor = m_allFacesToNeighbors[f];

    cellIndex c = f/Hex::numFacets;
    cellIndex cNeighbor = NO_ID;
    
    localIndex cellInBlock = getBlockCellFromManagerCell(c).second;
    // The neighbor cell may not be in the same CellBlock
    localIndex neighborCell = NO_ID;
    
    if (neighbor != NO_ID) {
      cNeighbor = neighbor/Hex::numFacets;
      neighborCell = getBlockCellFromManagerCell( cNeighbor ).second;
    }

    faceToElements[curFace][0] = cellInBlock;
    faceToElements[curFace][1] = neighborCell;
  }
  
}

// Using a set means bad performances and annoying code 
// TODO Change set to vector
void HexMeshConnectivityBuilder::getOneEdgeToFaces( 
  int edgeIndex, 
  std::vector< localIndex > const & allFacesToUniqueFace,
  std::set <localIndex> & faces ) const
{
  faces.clear();

  int first = m_uniqueEdges[edgeIndex];
  int last = m_allEdges.size();

  if( edgeIndex+1 < numEdges() )
  {
    last = m_uniqueEdges[edgeIndex+1];
  }

  // For each duplicate of this edge
  for( int i = first; i < last; ++i)
  {
    cellEdgeIndex id = m_allEdges[i].second;
    // Get the cell
    cellIndex c = id / Hex::numEdges;
    // Get the hex facet incident to the edge
    hexEdgeIndex e = id % Hex::numEdges;
    hexFacetIndex f0 = Hex::edgeAdjacentFacet[e][0];
    hexFacetIndex f1 = Hex::edgeAdjacentFacet[e][1];
    // Get the face index in allFaces 
    cellFaceIndex face0 = Hex::numFacets * c + f0;
    cellFaceIndex face1 = Hex::numFacets * c + f1;
    // Get the uniqueFaceId 
    localIndex uniqueFace0 = allFacesToUniqueFace[face0];
    localIndex uniqueFace1 = allFacesToUniqueFace[face1];

    faces.insert(uniqueFace0);
    faces.insert(uniqueFace1);
  }
}


void HexMeshConnectivityBuilder::computeFacesToEdges( ArrayOfArrays<localIndex> & faceToEdges) const 
{
  // 1 - Allocate
  faceToEdges.resize(0);
  // TODO Check if this resize or reserve the space for the inner arrays
  faceToEdges.resize( numFaces(), Hex::numEdgesPerFacet); 
  // In doubt resize inner arrays
  for (int i = 0; i < numFaces(); ++i)
  {
    faceToEdges.resizeArray(i, Hex::numEdgesPerFacet );
    // Initialize the values - Is there a simpler way to do this?
    faceToEdges[i][0] = NO_ID;
    faceToEdges[i][1] = NO_ID;
    faceToEdges[i][2] = NO_ID;
    faceToEdges[i][3] = NO_ID;
  }

  std::vector< localIndex >  const allFacesToUniqueFace = computeAllFacesToUniqueFace();

  for (unsigned int edgeIndex = 0; edgeIndex < m_uniqueEdges.size(); ++edgeIndex)
  {
    std::set<localIndex> faces;
    getOneEdgeToFaces( edgeIndex, allFacesToUniqueFace, faces );

    for( auto itr( faces.begin()); itr !=faces.end(); ++itr)
    { 
      if     ( faceToEdges[*itr][0] == NO_ID )  faceToEdges[*itr][0] = edgeIndex ;
      else if( faceToEdges[*itr][1] == NO_ID )  faceToEdges[*itr][1] = edgeIndex ;
      else if( faceToEdges[*itr][2] == NO_ID )  faceToEdges[*itr][2] = edgeIndex ;
      else if( faceToEdges[*itr][3] == NO_ID )  faceToEdges[*itr][3] = edgeIndex ;
      else GEOSX_ASSERT(" Change computeFacesToEdges implementation "); 
    }
  }
  
}

void HexMeshConnectivityBuilder::computeEdgesToFaces(ArrayOfSets<localIndex> & edgeToFaces ) const
{
  // 1 - Allocate - Without counting  -- Should we overallocate a bit ?
  // TODO Count for tet meshes one should count
  int numberOfFacesAroundEdge = 5; // In a hexahedral mesh this is enough
  // TODO How can we prevent problems with potentially bad quality and irregular hexahedral meshes?
  // Do we need to?
  edgeToFaces.resize( 0 );
  edgeToFaces.reserve( numEdges() );
  edgeToFaces.reserveValues( numEdges() * numberOfFacesAroundEdge );

  // Append and set the capacity of the individual arrays.
  for (int i = 0; i < numEdges(); ++i)
  {
    edgeToFaces.appendSet(numberOfFacesAroundEdge);
  }

  // 2 - Get the mapping
  std::vector< localIndex >  const allFacesToUniqueFace = computeAllFacesToUniqueFace();

  for (unsigned int edgeIndex = 0; edgeIndex < m_uniqueEdges.size(); ++edgeIndex)
  {
    std::set<localIndex> faces;
    getOneEdgeToFaces( edgeIndex, allFacesToUniqueFace, faces );
    
    for( auto itr( faces.begin()); itr !=faces.end(); ++itr)
    {
      edgeToFaces.insertIntoSet(edgeIndex, *itr);
    }
  }
  
}

// Allocation is managed by the CellBlock 
void HexMeshConnectivityBuilder::computeElementsToFacesOfCellBlocks() 
{
  for(unsigned int curFace = 0; curFace < m_uniqueFaces.size(); ++curFace)
  {
    cellFaceIndex id = m_uniqueFaces[curFace];

    cellIndex c = id / Hex::numFacets;
    hexFacetIndex face = id % Hex::numFacets;
    auto blockCell = getBlockCellFromManagerCell(c);

    getCellBlock( blockCell.first ).setElementToFaces( blockCell.second, face, curFace );

    cellFaceIndex idNeighbor = m_allFacesToNeighbors[id];
    if( idNeighbor != NO_ID)
    {
      cellIndex cNeighbor = idNeighbor / Hex::numFacets;
      hexFacetIndex faceNeighbor = idNeighbor % Hex::numFacets;
      auto blockCellNeighbor = getBlockCellFromManagerCell(cNeighbor) ;
      getCellBlock( blockCellNeighbor.first ).setElementToFaces( blockCellNeighbor.second, faceNeighbor, curFace );
    }
  }

  
}


// Allocation is managed by the CellBlock 
void HexMeshConnectivityBuilder::computeElementsToEdgesOfCellBlocks() 
{
  for (unsigned int edgeIndex = 0; edgeIndex < m_uniqueEdges.size(); ++edgeIndex)
  {
    unsigned int first = m_uniqueEdges[edgeIndex];
    unsigned int last = m_allEdges.size();

    if( edgeIndex+1 < m_uniqueEdges.size() ) {
      last = m_uniqueEdges[edgeIndex+1];
    }

    for(unsigned int i = first ; i < last; ++i)
    {
      cellEdgeIndex id = m_allEdges[i].second;
      cellIndex c = id / Hex::numEdges;
      hexEdgeIndex e = id % Hex::numEdges;
      auto blockCell = getBlockCellFromManagerCell(c);
      getCellBlock(blockCell.first).setElementToEdges(blockCell.second, e, edgeIndex);
    }
  }
  
}


/*********************************************************************************************************************/
/*********************************************************************************************************************/

HexCellBlockManager::HexCellBlockManager(string const &name, 
                                         dataRepository::Group *const parent) 
  :CellBlockManagerBase(name, parent)
{
  m_delegate = nullptr; 
}

HexCellBlockManager::~HexCellBlockManager() = default;


void HexCellBlockManager::buildMaps()
{
  // Create the MeshConnectivityBuilder after the CellBlocks are 
  // filled and cell types are known
  // TODO Change the instanciated class depending on the incoming types of cells
  m_delegate = std::unique_ptr<HexMeshConnectivityBuilder> (new HexMeshConnectivityBuilder( *this ));

  // If not called here - the number of faces and edges are not 
  // available resize EdgeManager and FaceManager.
  m_delegate->computeFaces();
  m_delegate->computeEdges();
  
  m_delegate->printDebugInformation(); 

  // Otherwise this are not called. Who should?
  m_delegate->computeElementsToFacesOfCellBlocks(); 
  m_delegate->computeElementsToEdgesOfCellBlocks();

  return ;
}

localIndex HexCellBlockManager::numEdges() const
{
  return m_delegate->numEdges();
}
localIndex HexCellBlockManager::numFaces() const
{
  return m_delegate->numFaces();
}

array2d<localIndex> HexCellBlockManager::getEdgeToNodes() const
{
  array2d<localIndex> result;
  m_delegate->computeEdgesToNodes(result);
  return result;
}

ArrayOfSets<localIndex> HexCellBlockManager::getEdgeToFaces() const
{
  ArrayOfSets<localIndex> result;
  m_delegate->computeEdgesToFaces(result);
  return result;
}
ArrayOfArrays<localIndex> HexCellBlockManager::getFaceToNodes() const
{
  ArrayOfArrays<localIndex> result;
  m_delegate->computeFacesToNodes(result);
  return result;
}
ArrayOfArrays<localIndex> HexCellBlockManager::getFaceToEdges() const
{
  ArrayOfArrays<localIndex> result;
  m_delegate->computeFacesToEdges(result);
  return result;
}
array2d<localIndex> HexCellBlockManager::getFaceToElements() const
{
  array2d<localIndex> result;
  m_delegate->computeFacesToElements(result);
  return result;
}
ArrayOfSets<localIndex> HexCellBlockManager::getNodeToEdges() const
{
  ArrayOfSets<localIndex> result;
  m_delegate->computeNodesToEdges(result);
  return result;
}
ArrayOfSets<localIndex> HexCellBlockManager::getNodeToFaces() const
{
  ArrayOfSets<localIndex> result;
  m_delegate->computeNodesToFaces(result);
  return result;
}
ArrayOfArrays<localIndex> HexCellBlockManager::getNodeToElements() const
{
  ArrayOfArrays<localIndex> result;
  m_delegate->computeNodesToElements(result);
  return result;
}

} // namespace geosx

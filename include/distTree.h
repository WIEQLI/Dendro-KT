/**
 * @file distTree.h
 * @author Masado Ishii, UofU SoC
 * @date 2019-10-04
 * @brief Struct to hold part of a distributed tree.
 */

#ifndef DENDRO_KT_DIST_TREE_H
#define DENDRO_KT_DIST_TREE_H


#include "treeNode.h"
#include "octUtils.h"
#include "mpi.h"

#include <functional>

namespace ot
{
  //TODO the DA class and the eleTreeIterator class should now take
  //references to DistTree instead of references to std::vector<TreeNode<T,dim>>

  /**
   * @brief Intermediate container for filtering trees before creating the DA.
   *
   * @note  DistTree takes ownership of the provided TreeNodes and empties the
   *        provided std::vector.
   *
   * @note  It is intended that, during construction of the DA, the tree
   *        vector held by DistTree will be destroyed.
   *
   * @note  Create a DistTree from a partitioned complete tree, i.e. taking
   *        union of the TreeNodes across all processors should be the entire
   *        unit hypercube. If you want to filter the domain to a subset of the
   *        unit hypercube, use DistTree to accomplish that.
   *
   * @note  DistTree remembers the front and back TreeNode from the original partition.
   * @note  The partition cannot be changed without creating a new DistTree.
   */
  template <typename T, unsigned int dim>
  class DistTree
  {
    public:
      // Member functions.
      //
      DistTree();
      DistTree(std::vector<TreeNode<T, dim>> &treePart);
      // Using default copy constructor and assignment operator.

      // generateGridHierarchy()
      void generateGridHierarchy(bool isFixedNumStrata,
                                 unsigned int lev,
                                 double loadFlexibility,
                                 MPI_Comm comm);

      // filterTree() has 2 overloads, depending on the type of your decider.
      void filterTree(
          const std::function<bool(const TreeNode<T, dim> &treeNodeElem)>
            &domainDecider);
      void filterTree(
          const std::function<bool(const double *elemPhysCoords, double elemPhysSize)>
            &domainDecider);

      void destroyTree();

      const std::function<bool(const TreeNode<T, dim> &treeNodeElem)>
        & getDomainDeciderTN() const;

      const std::function<bool(const double *elemPhysCoords, double elemPhysSize)>
        & getDomainDeciderPh() const;

      // Tree accessors.
      const std::vector<TreeNode<T, dim>> & getTreePartFiltered(int stratum = 0) const;
      size_t getOriginalTreePartSz() const;
      size_t getFilteredTreePartSz() const;
      TreeNode<T, dim> getTreePartFront() const;
      TreeNode<T, dim> getTreePartBack() const;


      int getNumStrata() { return m_numStrata; }


      // These deciders can be called directly.

      // Default domainDecider (treeNode)
      static bool defaultDomainDeciderTN(const TreeNode<T, dim> &tn)
      {
        bool isInside = true;

        const T domSz = 1u << m_uiMaxDepth;
        const T elemSz = 1u << (m_uiMaxDepth - tn.getLevel());
        #pragma unroll(dim)
        for (int d = 0; d < dim; d++)
          if (/*tn.getX(d) < 0 || */ tn.getX(d) > domSz - elemSz)  //Beware wraparound.
            isInside = false;

        return isInside;
      }

      // Default domainDecider (physical)
      static bool defaultDomainDeciderPh(const double * physCoords, double physSize)
      {
        bool isInside = true;

        #pragma unroll(dim)
        for (int d = 0; d < dim; d++)
          if (physCoords[d] < 0.0 || physCoords[d] + physSize > 1.0)
            isInside = false;

        return isInside;
      }

    protected:
      // Member variables.
      //
      std::function<bool(const TreeNode<T, dim> &treeNodeElem)> m_domainDeciderTN;
      std::function<bool(const double *elemPhysCoords, double elemPhysSize)> m_domainDeciderPh;

      bool m_usePhysCoordsDecider;

      size_t m_originalTreePartSz;
      size_t m_filteredTreePartSz;

      bool m_hasBeenFiltered;

      // Multilevel grids, finest first. Must initialize with at least one level.
      std::vector<std::vector<TreeNode<T, dim>>> m_gridStrata;
      std::vector<TreeNode<T, dim>> m_tpFrontStrata;
      std::vector<TreeNode<T, dim>> m_tpBackStrata;

      int m_numStrata;

      // Protected accessors return a reference to the 0th stratum.
      std::vector<TreeNode<T, dim>> &get_m_treePartFiltered() { return m_gridStrata[0]; }
      const std::vector<TreeNode<T, dim>> &get_m_treePartFiltered() const { return m_gridStrata[0]; }
      TreeNode<T, dim> &get_m_treePartFront() { return m_tpFrontStrata[0]; }
      const TreeNode<T, dim> &get_m_treePartFront() const { return m_tpFrontStrata[0]; }
      TreeNode<T, dim> &get_m_treePartBack() { return m_tpBackStrata[0]; }
      const TreeNode<T, dim> &get_m_treePartBack() const { return m_tpBackStrata[0]; }


      //
      // Intrinsic Deciders (not callable directly).
      //

      // If given a decider on phys coords, can still test treeNodes.
      bool conversionDomainDeciderTN(const TreeNode<T, dim> &tn)
      {
        double physCoords[dim];
        double physSize;
        treeNode2Physical(tn, physCoords, physSize);

        return m_domainDeciderPh(physCoords, physSize);
      }

      // If given a decider on treeNodes, can still test physCoords.
      bool conversionDomainDeciderPh(const double * physCoords, double physSize)
      {
        return m_domainDeciderTN(physical2TreeNode<T,dim>(physCoords, physSize));
      }
  };


  //
  // DistTree() - default constructor
  //
  template <typename T, unsigned int dim>
  DistTree<T, dim>::DistTree()
  : m_gridStrata(m_uiMaxDepth+1),
    m_tpFrontStrata(m_uiMaxDepth+1),
    m_tpBackStrata(m_uiMaxDepth+1),
    m_numStrata(0)
  {
    m_usePhysCoordsDecider = false;

    m_domainDeciderTN = DistTree::defaultDomainDeciderTN;
    m_domainDeciderPh = DistTree::defaultDomainDeciderPh;
    m_originalTreePartSz = 0;
    m_filteredTreePartSz = 0;

    m_hasBeenFiltered = false;
  }


  //
  // DistTree() - constructor
  //
  template <typename T, unsigned int dim>
  DistTree<T, dim>::DistTree(std::vector<TreeNode<T, dim>> &treePart)
  : m_gridStrata(m_uiMaxDepth+1),
    m_tpFrontStrata(m_uiMaxDepth+1),
    m_tpBackStrata(m_uiMaxDepth+1),
    m_numStrata(1)
  {
    m_usePhysCoordsDecider = false;

    m_domainDeciderTN = DistTree::defaultDomainDeciderTN;
    m_domainDeciderPh = DistTree::defaultDomainDeciderPh;
    m_originalTreePartSz = treePart.size();
    m_filteredTreePartSz = treePart.size();

    if (treePart.size())
    {
      get_m_treePartFront() = treePart.front();
      get_m_treePartBack() = treePart.back();
    }

    get_m_treePartFiltered().clear();
    std::swap(get_m_treePartFiltered(), treePart);  // Steal the tree vector.

    m_hasBeenFiltered = false;
  }


  //
  // destroyTree()
  //
  template <typename T, unsigned int dim>
  void DistTree<T, dim>::destroyTree()
  {
    for (std::vector<TreeNode<T, dim>> &gridStratum : m_gridStrata)
    {
      gridStratum.clear();
      gridStratum.shrink_to_fit();
    }
  }


  //
  // filterTree() (treeNode)
  //
  template <typename T, unsigned int dim>
  void DistTree<T, dim>::filterTree( const std::function<bool(const TreeNode<T, dim> &treeNodeElem)>
                                       &domainDecider)
  {
    m_usePhysCoordsDecider = false;
    m_domainDeciderTN = domainDecider;
    {
      using namespace std::placeholders;
      m_domainDeciderPh = std::bind(&DistTree<T,dim>::conversionDomainDeciderPh, this, _1, _2);
    }

    const size_t oldSz = get_m_treePartFiltered().size();
    size_t ii = 0;

    // Find first element to delete.
    while (ii < oldSz && domainDecider(get_m_treePartFiltered()[ii]))
      ii++;

    m_filteredTreePartSz = ii;

    // Keep finding and deleting elements.
    for ( ; ii < oldSz ; ii++)
      if (!domainDecider(get_m_treePartFiltered()[ii]))
        get_m_treePartFiltered()[m_filteredTreePartSz++] = std::move(get_m_treePartFiltered()[ii]);

    get_m_treePartFiltered().resize(m_filteredTreePartSz);

    m_hasBeenFiltered = true;
  }


  //
  // filterTree() (physical)
  //
  template <typename T, unsigned int dim>
  void DistTree<T, dim>::filterTree( const std::function<bool(const double *elemPhysCoords,
                                                              double elemPhysSize)>   &domainDecider)
  {
    m_usePhysCoordsDecider = true;
    m_domainDeciderPh = domainDecider;
    {
      using namespace std::placeholders;
      m_domainDeciderTN = std::bind(&DistTree<T,dim>::conversionDomainDeciderTN, this, _1);
    }

    // Intermediate variables to pass treeNode2Physical()-->domainDecider().
    double physCoords[dim];
    double physSize;

    const size_t oldSz = get_m_treePartFiltered().size();
    size_t ii = 0;

    // Find first element to delete.
    while (ii < oldSz
        && (treeNode2Physical(get_m_treePartFiltered()[ii], physCoords, physSize)
            , domainDecider(physCoords, physSize)))
      ii++;

    m_filteredTreePartSz = ii;

    // Keep finding and deleting elements.
    for ( ; ii < oldSz ; ii++)
      if ( !(treeNode2Physical(get_m_treePartFiltered()[ii], physCoords, physSize)
            , domainDecider(physCoords, physSize)) )
        get_m_treePartFiltered()[m_filteredTreePartSz++] = std::move(get_m_treePartFiltered()[ii]);

    get_m_treePartFiltered().resize(m_filteredTreePartSz);

    m_hasBeenFiltered = true;
  }



  //
  // getDomainDeciderTN()
  //
  template <typename T, unsigned int dim>
  const std::function<bool(const TreeNode<T, dim> &treeNodeElem)> &
      DistTree<T, dim>::getDomainDeciderTN() const
  {
    return m_domainDeciderTN;
  }


  //
  // getDomainDeciderPh()
  //
  template <typename T, unsigned int dim>
  const std::function<bool(const double *elemPhysCoords, double elemPhysSize)> &
      DistTree<T, dim>::getDomainDeciderPh() const
  {
    return m_domainDeciderPh;
  }


  //
  // getTreePartFiltered()
  //
  template <typename T, unsigned int dim>
  const std::vector<TreeNode<T, dim>> &
      DistTree<T, dim>::getTreePartFiltered(int stratum) const
  {
    return m_gridStrata[stratum];
  }


  //
  // getOriginalTreePartSz()
  //
  template <typename T, unsigned int dim>
  size_t DistTree<T, dim>::getOriginalTreePartSz() const
  {
    return m_originalTreePartSz;
  }


  //
  // getFilteredTreePartSz()
  //
  template <typename T, unsigned int dim>
  size_t DistTree<T, dim>::getFilteredTreePartSz() const
  {
    return m_filteredTreePartSz;
  }


  //
  // getTreePartFront()
  //
  template <typename T, unsigned int dim>
  TreeNode<T, dim> DistTree<T, dim>::getTreePartFront() const
  {
    return get_m_treePartFront();
  }


  //
  // getTreePartBack()
  //
  template <typename T, unsigned int dim>
  TreeNode<T, dim> DistTree<T, dim>::getTreePartBack() const
  {
    return get_m_treePartBack();
  }



}//namespace ot



#endif//DENDRO_KT_DIST_TREE_H
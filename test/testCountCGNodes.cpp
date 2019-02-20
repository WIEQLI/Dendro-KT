/*
 * testCountCGNodes.cpp
 *   Test sequential node enumeration methods.
 *
 * Masado Ishii  --  UofU SoC, 2019-02-19
 */


#include "treeNode.h"
#include "mathUtils.h"
#include "nsort.h"
/// #include "tsort.h"
/// #include "octUtils.h"

#include "hcurvedata.h"

#include <bitset>
#include <vector>

#include <iostream>


using T = unsigned int;

template <unsigned int dim>
using Tree = std::vector<ot::TreeNode<T,dim>>;

/**
 * @brief Example 1 is the minimal balanced tree in which the very center of the
 *        domain has pow(2,dim) elements of level endL.
 */ 
template <unsigned int dim>
struct Example1
{
  public:
    //
    // num_points()
    static constexpr unsigned int num_points(unsigned int endL, unsigned int order)
    {
      return (endL-2)*(intPow(4*order-1, dim) - intPow(2*order-1, dim)) + intPow(4*order+1, dim);
    }

    //
    // fill_tree()
    static void fill_tree(unsigned int endL, Tree<dim> &outTree)
    {
      constexpr unsigned char numCh = ot::TreeNode<T,dim>::numChildren;
      ot::TreeNode<T,dim> root;
      for (unsigned char ch = 0; ch < numCh; ch++)
      {
        generate_corner(root.getChildMorton(ch), numCh - ch, endL, outTree);
      }
    }

  private:
    /**@note Recursive method to generate a corner of the domain. */
    static void generate_corner(ot::TreeNode<T,dim> e, unsigned char ch, unsigned int endL, Tree<dim> &outTree)
    {
      constexpr unsigned char numCh = ot::TreeNode<T,dim>::numChildren;
      if (e.getLevel() >= endL)
        outTree.push_back(e);
      else
      {
        for (unsigned char otherCh = 0; otherCh < numCh; otherCh++)
        {
          if (otherCh != ch)
            outTree.push_back(e.getChildMorton(otherCh));
        }
        generate_corner(e.getChildMorton(ch), ch, endL, outTree);
      }
    }
};


/**
 * @brief Example 2 is the uniform grid with elements at level endL.
 */
template <unsigned int dim>
struct Example2
{
  public:
    //
    // num_points()
    static constexpr unsigned int num_points(unsigned int endL, unsigned int order)
    {
      return intPow(intPow(2,endL)*order + 1, dim);
    }

    //
    // fill_tree()
    static void fill_tree(unsigned int endL, Tree<dim> &outTree)
    {
      ot::TreeNode<T,dim> root;
      fill_tree(root, endL, outTree);
    }

  private:
    static void fill_tree(ot::TreeNode<T,dim> parent, unsigned int endL, Tree<dim> &outTree)
    {
      constexpr unsigned char numCh = ot::TreeNode<T,dim>::numChildren;
      if (parent.getLevel() >= endL)
        outTree.push_back(parent);
      else
      {
        for (unsigned char ch = 0; ch < numCh; ch++)
        {
          fill_tree(parent.getChildMorton(ch), endL, outTree);
        }
      }
    }
};


/**
 * @brief Example 3 is the minimal balanced tree with a fringe of elements of
 *        level endL all around the domain boundary.
 */
template <unsigned int dim>
struct Example3
{
  public:
    //
    // num_points()
    static unsigned int num_points(unsigned int endL, unsigned int order)
    {
      // Starts with a uniform grid of the finest level.
      unsigned int total = Example2<dim>::num_points(endL, order);

      // Summation (negative): Intermediate shells take points away.
      for (unsigned int l = 2; l <= endL - 1; l++)
      {
        total += intPow((intPow(2,l) - 2)*order + 1, dim);
        total -= intPow((intPow(2,l+1) - 4)*order + 1, dim);
      }

      return total;
    }

    //
    // fill_tree()
    static void fill_tree(unsigned int endL, Tree<dim> &outTree)
    {
      constexpr unsigned char numCh = ot::TreeNode<T,dim>::numChildren;
      ot::TreeNode<T,dim> root;
      for (unsigned char ch = 0; ch < numCh; ch++)
        subdivide_element(root.getChildMorton(ch), endL, outTree);
    }

  private:
    static void subdivide_element(ot::TreeNode<T,dim> parent, unsigned int endL, Tree<dim> &outTree)
    {
      constexpr unsigned char numCh = ot::TreeNode<T,dim>::numChildren;
      if (parent.getLevel() >= endL)
        outTree.push_back(parent);
      else
      {
        for (unsigned char ch = 0; ch < numCh; ch++)
        {
          ot::TreeNode<T,dim> f = parent.getChildMorton(ch);
          if (f.isTouchingDomainBoundary())
            subdivide_element(f, endL, outTree);
          else
            outTree.push_back(f);
        }
      }
    }
};




int main(int argc, char * argv[])
{

  // _InitializeHcurve(dim);
  // _DestroyHcurve();

  constexpr unsigned int dim = 3;
  const unsigned int endL = 7;
  const unsigned int order = 4;

  /// //
  /// // Test mathUtils intCombinations()
  /// //
  /// printf("5 choose ... %u %u %u %u %u %u\n",
  ///     intCombination(5u,0u),
  ///     intCombination(5u,1u),
  ///     intCombination(5u,2u),
  ///     intCombination(5u,3u),
  ///     intCombination(5u,4u),
  ///     intCombination(5u,5u));
  /// unsigned int dummy;
  /// printf("Sum 5 choose ... %u %u %u %u %u %u\n",
  ///     intCombinationSum(5u,0u, dummy),
  ///     intCombinationSum(5u,1u, dummy),
  ///     intCombinationSum(5u,2u, dummy),
  ///     intCombinationSum(5u,3u, dummy),
  ///     intCombinationSum(5u,4u, dummy),
  ///     intCombinationSum(5u,5u, dummy));


  /// //
  /// //Test CellType and TNPoint
  /// //
  /// // Height 1
  /// {
  ///   ot::TNPoint<T,dim> testPoint({1, 1, 4}, m_uiMaxDepth - 1);
  ///   ot::CellType<dim> testCellType = testPoint.get_cellType();
  ///   printf("({1,1,4}, 1) --> dim: %u  orient: %s\n",
  ///       testCellType.get_dim_flag(),
  ///       std::bitset<dim>(testCellType.get_orient_flag()).to_string().c_str());
  /// }
  /// // Height 3
  /// {
  ///   ot::TNPoint<T,dim> testPoint({1, 1, 4}, m_uiMaxDepth - 3);
  ///   ot::CellType<dim> testCellType = testPoint.get_cellType();
  ///   printf("({1,1,4}, 3) --> dim: %u  orient: %s\n",
  ///       testCellType.get_dim_flag(),
  ///       std::bitset<dim>(testCellType.get_orient_flag()).to_string().c_str());
  /// }


  //TODO actually execute the counting methods.
  unsigned int numPoints;
  Tree<dim> tree;

  numPoints = Example1<dim>::num_points(endL, order);
  Example1<dim>::fill_tree(endL, tree);
  printf("Example1: numPoints==%u, numElements==%lu.\n", numPoints, tree.size());
  tree.clear();

  numPoints = Example2<dim>::num_points(endL, order);
  Example2<dim>::fill_tree(endL, tree);
  printf("Example2: numPoints==%u, numElements==%lu.\n", numPoints, tree.size());
  tree.clear();

  numPoints = Example3<dim>::num_points(endL, order);
  Example3<dim>::fill_tree(endL, tree);
  printf("Example3: numPoints==%u, numElements==%lu.\n", numPoints, tree.size());
  tree.clear();

  return 0;
}

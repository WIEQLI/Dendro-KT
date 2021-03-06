/**
 * @file:tsort.h
 * @author: Masado Ishii  --  UofU SoC,
 * @date: 2018-12-03
 * @brief: Based on work by Milinda Fernando and Hari Sundar.
 * - Algorithms: SC18 "Comparison Free Computations..." TreeSort, TreeConstruction, TreeBalancing
 * - Code: Dendro4 [sfcSort.h] [construct.cpp]
 *
 * My contribution is to extend the data structures to 4 dimensions (or higher).
 */

#ifndef DENDRO_KT_SFC_TREE_H
#define DENDRO_KT_SFC_TREE_H

#include "treeNode.h"
#include <mpi.h>
#include <vector>
#include "hcurvedata.h"
#include "parUtils.h"
#include <stdio.h>

namespace ot
{

using LevI   = unsigned int;
using RankI  = DendroIntL;
using RotI   = int;
using ChildI = char;


//
// BucketInfo{}
//
// Buckets to temporarily represent (interior) nodes in the hyperoctree
// while we carry out breadth-first traversal. See distTreeSort().
template <typename T>
struct BucketInfo          // Adapted from Dendro4 sfcSort.h:132.
{
  RotI rot_id;
  LevI lev;
  T begin;
  T end;
};

template struct BucketInfo<RankI>;


// Wrapper around std::vector to act like a queue, plus it has a
// single barrier, which can be moved to the end of the queue at any time.
// TODO This should go in some other utilities header file.
template <typename T>
struct BarrierQueue
{
  // Usage:
  //
  // BarrierQueue q;
  // for (int i = 0; i < 5; i++) { q.enqueue(i); }
  // q.reset_barrier();
  // for (int i = 5; i < 10; i++) { q.enqueue(i); }
  //
  // int x;
  // while (q.dequeue(x)) { std::cout << x << ' '; }  // 0 1 2 3 4
  // q.reset_barrier();
  // while (q.dequeue(x)) { std::cout << x << ' '; }  // 5 6 7 8 9

  struct Range
  {
    typename std::vector<T>::iterator m_begin, m_end;
    typename std::vector<T>::iterator begin() { return m_begin; }
    typename std::vector<T>::iterator end() { return m_end; }
  };

  typename std::vector<T>::size_type b;  // An out-of-band barrier.
  std::vector<T> q;             // If you modify this, call reset_barrier() afterward.

  BarrierQueue() : q(), b(0) {};
  BarrierQueue(typename std::vector<T>::size_type s) : q(s), b(0) {};
  BarrierQueue(typename std::vector<T>::size_type s, T val) : q(s, val), b(0) {};
  void clear() { q.clear(); b = 0; }
  void reset_barrier() { b = q.size(); }
  void resize_back(typename std::vector<T>::size_type count) { q.resize(count + b); }
  typename std::vector<T>::size_type get_barrier() { return b; }
  typename std::vector<T>::size_type size() { return q.size(); }
  T front() { return *q.begin(); }
  T back() { return *q.end(); }
  Range leading() { return {q.begin(), q.begin() + b}; }
  Range trailing() { return {q.begin() + b, q.end()}; }
  void enqueue(T val) { q.push_back(val); }
  typename std::vector<T>::size_type dequeue(T &val)
  { if (b > 0) { val = q[0]; q.erase(q.begin()); } return (b > 0 ? b-- : 0); }
};

template <typename T, unsigned int D>
struct KeyFunIdentity_TN
{
  const TreeNode<T,D> &operator()(const TreeNode<T,D> &tn) { return tn; }
};

template <typename PointType>
struct KeyFunIdentity_Pt
{
  const PointType &operator()(const PointType &pt) { return pt; }
};




template <typename T, unsigned int D>
struct SFC_Tree
{

  // Notes:
  //   - This method operates in-place.
  //   - From sLev to eLev INCLUSIVE
  template <class PointType>   // = TreeNode<T,D>
  static void locTreeSort(PointType *points,
                          RankI begin, RankI end,
                          LevI sLev,
                          LevI eLev,
                          RotI pRot);            // Initial rotation, use 0 if sLev is 1.

  // Notes:
  //   - Allows the generality of a ``key function,''
  //        i.e. function to produce TreeNodes-like objects to sort by.
  //   - Otherwise, same as above except shuffles a parallel companion array along with the TreeNodes.
  template <class KeyFun, typename PointType, typename KeyType, typename Companion, bool useCompanions = true>
  static void locTreeSort(PointType *points, Companion *companions,
                          RankI begin, RankI end,
                          LevI sLev,
                          LevI eLev,
                          RotI pRot,            // Initial rotation, use 0 if sLev is 1.
                          KeyFun keyfun);

  // Notes:
  //   - outSplitters contains both the start and end of children at level `lev'
  //     This is to be consistent with the Dendro4 SFC_bucketing().
  //
  //     One difference is that here the buckets are ordered by the SFC
  //     (like the returned data is ordered) and so `outSplitters' should be
  //     monotonically increasing; whereas in Dendro4 SFC_bucketing(), the splitters
  //     are in permuted order.
  //
  //   - The size of outSplitters is 2+numChildren, which are splitters for
  //     1+numChildren buckets. The leading bucket holds ancestors and the
  //     remaining buckets are for children.
  static void SFC_bucketing(TreeNode<T,D> *points,
                          RankI begin, RankI end,
                          LevI lev,
                          RotI pRot,
                          std::array<RankI, 1+TreeNode<T,D>::numChildren> &outSplitters,
                          RankI &outAncStart,
                          RankI &outAncEnd);

  /**
   * @tparam KeyFun KeyType KeyFun::operator()(PointType);
   * @tparam KeyType must support the public interface of TreeNode<T,D>.
   * @tparam PointType passive data type.
   * @param ancestorsFirst If true, ancestor bucket precedes all siblings, else follows all siblings.
   */
  // Notes:
  //   - Buckets points based on TreeNode "keys" generated by applying keyfun(point).
  template <class KeyFun, typename PointType, typename KeyType>
  static void SFC_bucketing_impl(PointType *points,
                          RankI begin, RankI end,
                          LevI lev,
                          RotI pRot,
                          KeyFun keyfun,
                          bool separateAncestors,
                          bool ancestorsFirst,
                          std::array<RankI, 1+TreeNode<T,D>::numChildren> &outSplitters,
                          RankI &outAncStart,
                          RankI &outAncEnd);


  // Notes:
  //   - Same as above except shuffles a parallel companion array along with the TreeNodes.
  //   - In actuality the above version calls this one.
  template <class KeyFun, typename PointType, typename KeyType, typename Companion, bool useCompanions = true>
  static void SFC_bucketing_general(PointType *points, Companion* companions,
                          RankI begin, RankI end,
                          LevI lev,
                          RotI pRot,
                          KeyFun keyfun,
                          bool separateAncestors,
                          bool ancestorsFirst,
                          std::array<RankI, 1+TreeNode<T,D>::numChildren> &outSplitters,
                          RankI &outAncStart,
                          RankI &outAncEnd);


  /**
   * @tparam KeyFun KeyType KeyFun::operator()(PointType);
   * @tparam KeyType must support the public interface of TreeNode<T,D>.
   * @tparam PointType passive data type.
   * @param ancestorsFirst If true, ancestor bucket precedes all siblings, else follows all siblings.
   */
  // Notes:
  //   - Buckets points based on TreeNode "keys" generated by applying keyfun(point).
  //   - Same parameters as SFC_bucketing_impl, except does not move points, hence read-only.
  template <class KeyFun, typename PointType, typename KeyType>
  static void SFC_locateBuckets_impl(const PointType *points,
                          RankI begin, RankI end,
                          LevI lev,
                          RotI pRot,
                          KeyFun keyfun,
                          bool separateAncestors,
                          bool ancestorsFirst,
                          std::array<RankI, 1+TreeNode<T,D>::numChildren> &outSplitters,
                          RankI &outAncStart,
                          RankI &outAncEnd);



  // Notes:
  //   - Same parameters as SFC_bucketing, except does not move points.
  //   - This method is read only.
  //   TODO insert this method in both versions of SFC_bucketing to remove duplicate code.
  //   TODO get rid of old code
  static void SFC_locateBuckets(const TreeNode<T,D> *points,
                          RankI begin, RankI end,
                          LevI lev,
                          RotI pRot,
                          std::array<RankI, 2+TreeNode<T,D>::numChildren> &outSplitters);


  // Notes:
  //   - points will be replaced/resized with globally sorted data.
  static void distTreeSort(std::vector<TreeNode<T,D>> &points,
                           double loadFlexibility,
                           MPI_Comm comm);

  // This method does most of the work for distTreeSort and distTreeConstruction.
  // It includes the breadth-first global sorting phase and Alltoallv()
  // but does not sort locally.
  //
  // pFinalOctants is an output parameter of the global refinement structure.
  // If it is NULL then it is unused.
  // If it is not NULL then it is cleared and filled with the output data.
  //
  // Notes:
  //   - points will be replaced/resized with globally sorted data.
  static void distTreePartition(std::vector<TreeNode<T,D>> &points,
                           double loadFlexibility,
                           MPI_Comm comm);

  //
  // treeBFTNextLevel()
  //   Takes the queue of BucketInfo in a breadth-first traversal, and finishes
  //   processing the current level. Each dequeued bucket is subdivided,
  //   and the sub-buckets in the corresponding range of `points` are sorted.
  //   Then the sub-buckets are initialized and enqueued to the back.
  //
  static void treeBFTNextLevel(TreeNode<T,D> *points,
      std::vector<BucketInfo<RankI>> &bftQueue);


  // -------------------------------------------------------------

  // Notes:
  //   - (Sub)tree will be built by appending to `tree'.
  static void locTreeConstruction(TreeNode<T,D> *points,
                                  std::vector<TreeNode<T,D>> &tree,
                                  RankI maxPtsPerRegion,
                                  RankI begin, RankI end,
                                  LevI sLev,
                                  LevI eLev,
                                  RotI pRot,
                                  TreeNode<T,D> pNode);

  static void distTreeConstruction(std::vector<TreeNode<T,D>> &points,
                                   std::vector<TreeNode<T,D>> &tree,
                                   RankI maxPtsPerRegion,
                                   double loadFlexibility,
                                   MPI_Comm comm);

  // Removes duplicate/ancestor TreeNodes from a sorted list of TreeNodes.
  // Notes:
  //   - Removal is done in a single pass in-place. The vector may be shrunk.
  static void locRemoveDuplicates(std::vector<TreeNode<T,D>> &tnodes);

  // Notes:
  //   - Nodes only removed if strictly equal to other nodes. Ancestors retained.
  static void locRemoveDuplicatesStrict(std::vector<TreeNode<T,D>> &tnodes);


  // -------------------------------------------------------------

  /**
   * @brief Create auxiliary octants in bottom-up order to close the 2:1-balancing constraint.
   */
  static void propagateNeighbours(std::vector<TreeNode<T,D>> &tree);

  // Notes:
  //   - Constructs a tree based on distribution of points, then balances and completes.
  //   - Initializes tree with balanced complete tree.
  static void locTreeBalancing(std::vector<TreeNode<T,D>> &points,
                               std::vector<TreeNode<T,D>> &tree,
                               RankI maxPtsPerRegion);

  static void distTreeBalancing(std::vector<TreeNode<T,D>> &points,
                                   std::vector<TreeNode<T,D>> &tree,
                                   RankI maxPtsPerRegion,
                                   double loadFlexibility,
                                   MPI_Comm comm);

  // -------------------------------------------------------------

  /**
   * @brief Given partition splitters and a list of (unordered) points, finds every block that contains at least some of the points.
   * @param splitters an array that holds the leading boundary of each block.
   * @note Assumes that the points are at the deepest level.
   * @note Assumes that the partition splitters are already SFC-sorted.
   */
  // Use this one.
  static void getContainingBlocks(TreeNode<T,D> *points,
                                  RankI begin, RankI end,
                                  const TreeNode<T,D> *splitters,
                                  int numSplitters,
                                  std::vector<int> &outBlocks);

  // Recursive implementation.
  static void getContainingBlocks(TreeNode<T,D> *points,
                                  RankI begin, RankI end,
                                  const TreeNode<T,D> *splitters,
                                  RankI sBegin, RankI sEnd,
                                  LevI lev, RotI pRot,
                                  int &numPrevBlocks,
                                  const int startSize,
                                  std::vector<int> &outBlocks);

};

// Template instantiations.
template struct SFC_Tree<unsigned int, 2>;
template struct SFC_Tree<unsigned int, 3>;
template struct SFC_Tree<unsigned int, 4>;

} // namespace ot

#include "tsort.tcc"

#endif // DENDRO_KT_SFC_TREE_H

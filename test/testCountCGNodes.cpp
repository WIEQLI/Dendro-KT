/*
 * testCountCGNodes.cpp
 *   Test sequential node enumeration methods.
 *
 * Masado Ishii  --  UofU SoC, 2019-02-19
 */


#include "testAdaptiveExamples.h"

#include "treeNode.h"
#include "mathUtils.h"
#include "nsort.h"

#include "hcurvedata.h"

#include <bitset>
#include <vector>

#include <iostream>


/// using T = unsigned int;
/// 
/// template <unsigned int dim>
/// using Tree = std::vector<ot::TreeNode<T,dim>>;
/// 
/// template <unsigned int dim>
/// using NodeList = std::vector<ot::TNPoint<T,dim>>;

/// template<typename X>
/// void distPrune(std::vector<X> &list, MPI_Comm comm)
/// {
///   int nProc, rProc;
///   MPI_Comm_rank(comm, &rProc);
///   MPI_Comm_size(comm, &nProc);
/// 
///   const int listSize = list.size();
///   const int baseSeg = listSize / nProc;
///   const int remainder = listSize - baseSeg * nProc;
///   const int myStart = rProc * baseSeg + (rProc < remainder ? rProc : remainder);
///   const int mySeg = baseSeg + (rProc < remainder ? 1 : 0);
/// 
///   /// fprintf(stderr, "[%d] listSize==%d, myStart==%d, mySeg==%d\n", rProc, listSize, myStart, mySeg);
/// 
///   list.erase(list.begin(), list.begin() + myStart);
///   list.resize(mySeg);
/// }


template <unsigned int dim, unsigned int order>
void testExample(const char *msgPrefix, unsigned int expected, Tree<dim> &tree, const bool RunDistributed, double tol, MPI_Comm comm);


int main(int argc, char * argv[])
{
  MPI_Init(&argc, &argv);

  const bool RunDistributed = true;  // Switch between sequential and distributed.
  int nProc, rProc;
  MPI_Comm_rank(MPI_COMM_WORLD, &rProc);
  MPI_Comm_size(MPI_COMM_WORLD, &nProc);
  MPI_Comm comm = MPI_COMM_WORLD;

  constexpr unsigned int dim = 2;
  const unsigned int endL = 3;
  const unsigned int order = 3;

  double tol = 0.05;

  _InitializeHcurve(dim);

  unsigned int numPoints;
  Tree<dim> tree;

  char msgPrefix[50];

  // -------------

  if (rProc == 0)
  {
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
  }

  // -------------

  // Example1
  Example1<dim>::fill_tree(endL, tree);
  sprintf(msgPrefix, "Example1");
  testExample<dim,order>(msgPrefix, Example1<dim>::num_points(endL, order), tree, RunDistributed, tol, comm);
  tree.clear();

  // Example2
  Example2<dim>::fill_tree(endL, tree);
  sprintf(msgPrefix, "Example2");
  testExample<dim,order>(msgPrefix, Example2<dim>::num_points(endL, order), tree, RunDistributed, tol, comm);
  tree.clear();

  // Example3
  Example3<dim>::fill_tree(endL, tree);
  sprintf(msgPrefix, "Example3");
  testExample<dim,order>(msgPrefix, Example3<dim>::num_points(endL, order), tree, RunDistributed, tol, comm);
  tree.clear();

  _DestroyHcurve();

  MPI_Finalize();

  return 0;
}



//
// testExample()
//
template <unsigned int dim, unsigned int order>
void testExample(const char *msgPrefix, unsigned int expected, Tree<dim> &tree, const bool RunDistributed, double tol, MPI_Comm comm)
{
  NodeList<dim> nodeListExterior;
  NodeList<dim> nodeListInterior;
  NodeList<dim> nodeListCombined;

  ot::RankI numUniqueInteriorNodes;
  ot::RankI numUniqueExteriorNodes;
  ot::RankI numUniqueNodes;

  ot::RankI numUniqueCombinedNodes;


  if (RunDistributed)
  {
    distPrune(tree, comm);
    ot::SFC_Tree<T,dim>::distTreeSort(tree, tol, comm);
  }
  for (const ot::TreeNode<T,dim> &tn : tree)
  {
    ot::Element<T,dim>(tn).appendInteriorNodes(order, nodeListInterior);
    ot::Element<T,dim>(tn).appendExteriorNodes(order, nodeListExterior);
    ot::Element<T,dim>(tn).appendNodes(order, nodeListCombined);
  }
  numUniqueInteriorNodes = nodeListInterior.size();
  if (RunDistributed)
  {
    numUniqueExteriorNodes = ot::SFC_NodeSort<T,dim>::dist_countCGNodes(nodeListExterior, order, &(tree.front()), &(tree.back()), comm);
    ot::RankI globInterior = 0;
    par::Mpi_Allreduce(&numUniqueInteriorNodes, &globInterior, 1, MPI_SUM, comm);
    numUniqueInteriorNodes = globInterior;
    numUniqueCombinedNodes = ot::SFC_NodeSort<T,dim>::dist_countCGNodes(nodeListCombined, order, &(tree.front()), &(tree.back()), comm);
  }
  else
  {
    numUniqueExteriorNodes = ot::SFC_NodeSort<T,dim>::countCGNodes(&(*nodeListExterior.begin()), &(*nodeListExterior.end()), order);
    numUniqueCombinedNodes = ot::SFC_NodeSort<T,dim>::countCGNodes(&(*nodeListCombined.begin()), &(*nodeListCombined.end()), order);
  }
  numUniqueNodes = numUniqueInteriorNodes + numUniqueExteriorNodes;
  printf("%s: Algorithm says # points == %s%u%s \t [Int:%u] [Ext:%u] [Comb:%u].\n", msgPrefix, (numUniqueNodes == expected ? GRN : RED), numUniqueNodes, NRM, numUniqueInteriorNodes, numUniqueExteriorNodes, numUniqueCombinedNodes);
  nodeListInterior.clear();
  nodeListExterior.clear();
  nodeListCombined.clear();
}



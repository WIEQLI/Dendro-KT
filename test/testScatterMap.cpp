/*
 * testScatterMap.cpp
 *   Test consistency of scater/gather maps after dist_countCGNodes().
 *
 * Masado Ishii  --  UofU SoC, 2019-03-14
 */



#include "testAdaptiveExamples.h"

#include "treeNode.h"
#include "nsort.h"

#include "hcurvedata.h"

#include <vector>

#include <iostream>



template<typename X>
void distPrune(std::vector<X> &list, MPI_Comm comm)
{
  int nProc, rProc;
  MPI_Comm_rank(comm, &rProc);
  MPI_Comm_size(comm, &nProc);

  const int listSize = list.size();
  const int baseSeg = listSize / nProc;
  const int remainder = listSize - baseSeg * nProc;
  const int myStart = rProc * baseSeg + (rProc < remainder ? rProc : remainder);
  const int mySeg = baseSeg + (rProc < remainder ? 1 : 0);

  list.erase(list.begin(), list.begin() + myStart);
  list.resize(mySeg);
}


int main(int argc, char * argv[])
{
  MPI_Init(&argc, &argv);

  int nProc, rProc;
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Comm_rank(comm, &rProc);
  MPI_Comm_size(comm, &nProc);

  constexpr unsigned int dim = 3;
  const unsigned int endL = 3;
  const unsigned int order = 2;

  double tol = 0.05;

  _InitializeHcurve(dim);

  unsigned int numPoints;
  Tree<dim> tree;
  NodeList<dim> nodeListExterior;
  /// NodeList<dim> nodeListInterior;

  ot::RankI numUniqueInteriorNodes;
  ot::RankI numUniqueExteriorNodes;
  ot::RankI numUniqueNodes;

  ot::ScatterMap scatterMap;
  ot::GatherMap gatherMap;

  // ----------------------------

  // Example3
  Example3<dim>::fill_tree(endL, tree);
  distPrune(tree, comm);
  ot::SFC_Tree<T,dim>::distTreeSort(tree, tol, comm);
  for (const ot::TreeNode<T,dim> &tn : tree)
  {
    /// ot::Element<T,dim>(tn).appendInteriorNodes(order, nodeListInterior);
    ot::Element<T,dim>(tn).appendExteriorNodes(order, nodeListExterior);
  }
  /// numUniqueInteriorNodes = nodeListInterior.size();
  numUniqueExteriorNodes = ot::SFC_NodeSort<T,dim>::dist_countCGNodes(nodeListExterior, order, tree.data(), scatterMap, gatherMap, comm);
  /// ot::RankI globInterior = 0;
  /// par::Mpi_Allreduce(&numUniqueInteriorNodes, &globInterior, 1, MPI_SUM, comm);
  /// numUniqueInteriorNodes = globInterior;
  numUniqueNodes = /*numUniqueInteriorNodes +*/ numUniqueExteriorNodes;

  // ----------------------------

  // Send and receive some stuff, verify the ghost segments have allocated space
  // in order of increasing processor rank.

  // Allocate space for local data + ghost segments on either side.
  std::vector<int> dataArray(gatherMap.m_totalCount);
  int * const myDataBegin = dataArray.data() + gatherMap.m_locOffset;
  int * const myDataEnd = myDataBegin + gatherMap.m_locCount;

  std::vector<int> sendBuf(scatterMap.m_map.size());

  // Initialize values of our local data to rProc. Those that should not be sent are negative.
  for (int * myDataIter = myDataBegin; myDataIter < myDataEnd; myDataIter++)
    *myDataIter = -rProc;
  for (ot::RankI ii = 0; ii < sendBuf.size(); ii++)
    myDataBegin[scatterMap.m_map[ii]] = rProc;

  // Stage send data.
  for (ot::RankI ii = 0; ii < sendBuf.size(); ii++)
    sendBuf[ii] = myDataBegin[scatterMap.m_map[ii]];

  // Send/receive data.
  std::vector<MPI_Request> requestSend(scatterMap.m_sendProc.size());
  std::vector<MPI_Request> requestRecv(gatherMap.m_recvProc.size());
  MPI_Status status;

  for (int sIdx = 0; sIdx < scatterMap.m_sendProc.size(); sIdx++)
    par::Mpi_Isend(sendBuf.data() + scatterMap.m_sendOffsets[sIdx],   // Send.
        scatterMap.m_sendCounts[sIdx],
        scatterMap.m_sendProc[sIdx], 0, comm, &requestSend[sIdx]);

  for (int rIdx = 0; rIdx < gatherMap.m_recvProc.size(); rIdx++)
    par::Mpi_Irecv(dataArray.data() + gatherMap.m_recvOffsets[rIdx],  // Recv.
        gatherMap.m_recvCounts[rIdx],
        gatherMap.m_recvProc[rIdx], 0, comm, &requestRecv[rIdx]);

  for (int sIdx = 0; sIdx < scatterMap.m_sendProc.size(); sIdx++)     // Wait sends.
    MPI_Wait(&requestSend[sIdx], &status);
  for (int rIdx = 0; rIdx < gatherMap.m_recvProc.size(); rIdx++)      // Wait recvs.
    MPI_Wait(&requestRecv[rIdx], &status);

  // Check that everything got to the proper place.
  int success = true;
  int lastVal = dataArray[0];
  int ii;
  for (ii = 0; ii < dataArray.size(); ii++)
  {
    int val = dataArray[ii];

    /// fprintf(stderr, "%d(%d)  ", rProc, val);

    if (val < 0 && -val != rProc)
    {
      success = false;
      break;
    }
    if (val < 0)
      val = -val;
    if (val < lastVal)
    {
      success = false;
      break;
    }

    /// if (val != rProc)
    /// {
    ///   for (int k = 0; k < rProc; k++)
    ///     fprintf(stderr, "\t");
    ///   fprintf(stderr, "[%d](%d)\n", rProc, val);
    /// }
  }
  fprintf(stderr, "  [%d] >>Exiting loop<<  Success? %s\n", rProc, (success ? "Yes" : "NO, FAILED"));
  if (!success)
    fprintf(stderr, "[%d] Failed at dataArray[%d].\n", rProc, ii);

  // ----------------------------

  tree.clear();
  /// nodeListInterior.clear();
  nodeListExterior.clear();

  // ----------------------------

  _DestroyHcurve();

  MPI_Finalize();

  return 0;
}
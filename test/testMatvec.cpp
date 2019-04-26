/*
 * testScatterMap.cpp
 *   Test consistency of scater/gather maps after dist_countCGNodes().
 *
 * Masado Ishii  --  UofU SoC, 2019-03-14
 */



#include "testAdaptiveExamples.h"

#include "treeNode.h"
#include "nsort.h"
#include "parUtils.h"
#include "feMatrix.h"
/// #include "feVector.h"
#include "hcurvedata.h"

#include <iostream>
#include <functional>
#include <vector>
#include <algorithm>



template <unsigned int dim>
bool testInstances(MPI_Comm comm, unsigned int depth, unsigned int order);

template <unsigned int dim>
bool testMatching(MPI_Comm comm, unsigned int depth, unsigned int order);

// ==============================
// main()
// ==============================
int main(int argc, char * argv[])
{
  MPI_Init(&argc, &argv);

  MPI_Comm comm = MPI_COMM_WORLD;

  int rProc, nProc;
  MPI_Comm_rank(comm, &rProc);
  MPI_Comm_size(comm, &nProc);

  const char * usageString = "Usage: %s dim depth order\n";
  unsigned int inDim, inDepth, inOrder;

  if (argc < 4)
  {
    if (!rProc)
      printf(usageString, argv[0]);
    exit(1);
  }
  else
  {
    inDim   = strtol(argv[1], NULL, 0);
    inDepth = strtol(argv[2], NULL, 0);
    inOrder = strtol(argv[3], NULL, 0);
  }

  _InitializeHcurve(inDim);

  if (!rProc)
    printf("Test results: ");

  int result_testInstances, globResult_testInstances;
  switch (inDim)
  {
    case 2: result_testInstances = testInstances<2>(comm, inDepth, inOrder); break;
    case 3: result_testInstances = testInstances<3>(comm, inDepth, inOrder); break;
    case 4: result_testInstances = testInstances<4>(comm, inDepth, inOrder); break;
    default: if (!rProc) printf("Dimension not supported.\n"); exit(1); break;
  }
  par::Mpi_Reduce(&result_testInstances, &globResult_testInstances, 1, MPI_SUM, 0, comm);
  if (!rProc)
    printf("\t[testInstances](%s)", (globResult_testInstances ? GRN "success" NRM : RED "FAILURE" NRM));

  if(!rProc)
    printf("\n");

  _DestroyHcurve();

  MPI_Finalize();

  return 0;
}


//
// myConcreteFeMatrix
//
template <unsigned int dim>
class myConcreteFeMatrix : public feMatrix<myConcreteFeMatrix<dim>, dim>
{
  using T = myConcreteFeMatrix;
  public:
    using feMatrix<T,dim>::feMatrix;
    virtual void elementalMatVec(const VECType *in, VECType *out, double *coords, double scale) override;
    /// virtual void preMatVec
};

template <unsigned int dim>
void myConcreteFeMatrix<dim>::elementalMatVec(const VECType *in, VECType *out, double *coords, double scale)
{
  const RefElement* refEl=feMat<dim>::m_uiOctDA->getReferenceElement();

  const unsigned int eleOrder=refEl->getOrder();
  const unsigned int nPe=intPow(eleOrder+1, dim);

  // Dummy identity.
  for (int ii = 0; ii < nPe; ii++)
    out[ii] = in[ii];
}


/// //
/// // myConcreteFeVector
/// //
/// template <unsigned int dim>
/// class myConcreteFeVector : public feVector<myConcreteFeVector<dim>, dim>
/// {
///   using T = myConcreteFeVector;
///   public:
///     static constexpr unsigned int order = 1;   // Only support a static order for now.  //TODO add order paramter to elementalMatVec()
///     using feVector<T,dim>::feVector;
///     virtual void elementalComputeVec(const VECType *in, VECType *out, double *coords, double scale) override;
/// };
/// 
/// template <unsigned int dim>
/// void myConcreteFeVector<dim>::elementalComputeVec(const VECType *in, VECType *out, double *coords, double scale)
/// {
///   // Dummy identity.
///   const unsigned int nPe = intPow(order + 1, dim);
///   for (int ii = 0; ii < nPe; ii++)
///       out[ii] = in[ii];
/// }





template <unsigned int dim>
bool testInstances(MPI_Comm comm, unsigned int depth, unsigned int order)
{
  bool testResult = true;

  int rProc, nProc;
  MPI_Comm_rank(comm, &rProc);
  MPI_Comm_size(comm, &nProc);

  const unsigned int numPtsPerProc = (1u<<depth) / nProc;
  const double loadFlexibility = 0.3;

  // Uniform grid ODA.
  ot::DA<dim> *octDA = new ot::DA<dim>(comm, order, numPtsPerProc, loadFlexibility);

  std::vector<double> vecIn, vecOut;
  octDA->createVector(vecIn, false, false, order);
  octDA->createVector(vecOut, false, false, order);

  // Fill the in vector with all ones.
  std::fill(vecIn.begin(), vecIn.end(), 1.0);
  /// std::fill(vecOut.begin(), vecOut.end(), 1.0);

  myConcreteFeMatrix<dim> mat(octDA, 1);
  mat.matVec(&(*vecIn.cbegin()), &(*vecOut.begin()), 1.0);

  // Check that the output vector contains the grid intersection degree at each node.
  const ot::TreeNode<unsigned int, dim> *nodeCoords = octDA->getTNCoords() + octDA->getLocalNodeBegin();
  for (unsigned int ii = 0; testResult && ii < vecOut.size(); ii++)
  {
    unsigned int domMask = (1u << m_uiMaxDepth) - 1;
    unsigned int gridMask = (1u << (m_uiMaxDepth - nodeCoords[ii].getLevel())) - 1;
    unsigned int interxDeg = dim;
    for (int d = 0; d < dim; d++)
      interxDeg -= ((bool)(gridMask & nodeCoords[ii].getX(d)) || !(bool)(domMask & nodeCoords[ii].getX(d)));

    testResult &= (vecOut[ii] == (1u << interxDeg));
  }

  octDA->destroyVector(vecIn);
  octDA->destroyVector(vecOut);
  delete octDA;

  return testResult;
}




template <unsigned int dim>
bool testMatching(MPI_Comm comm, unsigned int depth, unsigned int order)
{

}

/**
 * @author Masado Ishii
 */

#ifndef DENDRO_KT_SETDIAG_H
#define DENDRO_KT_SETDIAG_H

#include "sfcTreeLoop_matvec.h"

namespace fem
{
  template <typename da>
  using EleSetT = std::function<void(da *out, unsigned int ndofs, const double *coords, double scale)>;


  template <typename DofT, typename TN, typename RE>
  void locSetDiag(DofT* vecOut, unsigned int ndofs, const TN *coords, unsigned int sz, const TN &partFront, const TN &partBack, EleSetT<DofT> eleSet, double scale, const RE* refElement)
  {
    // Initialize output vector to 0.
    std::fill(vecOut, vecOut + ndofs*sz, 0);

    using C = typename TN::coordType;  // If not unsigned int, error.
    constexpr unsigned int dim = TN::coordDim;
    const unsigned int eleOrder = refElement->getOrder();
    const unsigned int npe = intPow(eleOrder+1, dim);

    std::vector<DofT> leafResult(ndofs*npe, 0.0);

    constexpr bool noVisitEmpty = false;
    ot::MatvecBaseOut<dim, DofT> treeLoopOut(sz, ndofs, eleOrder, noVisitEmpty, 0, coords, partFront, partBack);

    while (!treeloop.isFinished())
    {
      if (treeloop.isPre() && treeloop.subtreeInfo().isLeaf())
      {
        const double * nodeCoordsFlat = treeloop.subtreeInfo().getNodeCoords();

        eleSet(&(*leafResult.begin()), ndofs, nodeCoordsFlat, scale);

        treeloop.subtreeInfo().overwriteNodeValsOut(&(*leafResult.begin()));

        treeloop.next();
      }
      else
        treeloop.step();
    }

    size_t writtenSz = treeloop.finalize(vecOut);

    if (sz > 0 && writtenSz == 0)
      std::cerr << "Warning: locSetDiag() did not write any data! Loop misconfigured?\n";
  }




}//namespace fem

#endif// DENDRO_KT_SETDIAG_H
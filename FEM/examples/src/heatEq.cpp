//
// Created by milinda on 11/21/18.
//


#include "treeNode.h"
#include "mpi.h"
/// #include "genPts_par.h"
#include "tsort.h"
/// #include "mesh.h"
#include "dendro.h"
/// #include "dendroIO.h"
#include "octUtils.h"
#include "functional"
/// #include "fdCoefficient.h"
/// #include "stencil.h"
/// #include "rkTransport.h"
#include "refel.h"
/// #include "operators.h"
/// #include "cg.h"
#include "heatMat.h"
#include "heatVec.h"

int main (int argc, char** argv)
{
    constexpr unsigned int dim = 3;
    unsigned int m_uiDim = dim;

    MPI_Init(&argc, &argv);
    MPI_Comm comm = MPI_COMM_WORLD;

    int rank, npes;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &npes);

    if (argc < 4) {
        if (!rank)
            std::cout << "Usage: " << argv[0]
                      << " maxDepth wavelet_tol partition_tol eleOrder "
                      << std::endl;
        return 0;
    }

    m_uiMaxDepth = atoi(argv[1]);
    double wavelet_tol = atof(argv[2]);
    double partition_tol = atof(argv[3]);
    unsigned int eOrder = atoi(argv[4]);


    double tBegin = 0, tEnd = 10, th = 0.01;

    if (!rank) {
        std::cout << YLW << "maxDepth: " << m_uiMaxDepth << NRM << std::endl;
        std::cout << YLW << "wavelet_tol: " << wavelet_tol << NRM << std::endl;
        std::cout << YLW << "partition_tol: " << partition_tol << NRM << std::endl;
        std::cout << YLW << "eleOrder: " << eOrder << NRM << std::endl;

    }

    _InitializeHcurve(m_uiDim);
    RefElement refEl(m_uiDim,eOrder);


    enum VAR{M_UI_U=0,M_UI_F,M_UI_MF};
    const char * VAR_NAMES[]={"m_uiU","m_uiFrhs","m_uiMFrhs"};
    const unsigned int DOF=3;


    Point<dim> grid_min(0, 0, 0);
    Point<dim> grid_max(1, 1, 1);

    Point<dim> domain_min(-0.5,-0.5,-0.5);
    Point<dim> domain_max(0.5,0.5,0.5);

    double Rg_x=(grid_max.x()-grid_min.x());
    double Rg_y=(grid_max.y()-grid_min.y());
    double Rg_z=(grid_max.z()-grid_min.z());

    double Rd_x=(domain_max.x()-domain_min.x());
    double Rd_y=(domain_max.y()-domain_min.y());
    double Rd_z=(domain_max.z()-domain_min.z());

    const Point<dim> d_min=domain_min;
    const Point<dim> d_max=domain_max;

    const Point<dim> g_min=grid_min;
    const Point<dim> g_max=grid_max;

    std::function<void(double,double,double,double*)> f_rhs =[d_min,d_max,g_min,g_max,Rg_x,Rg_y,Rg_z,Rd_x,Rd_y,Rd_z](const double x,const double y,const double z,double* var){
        var[0]=(-12*M_PI*M_PI*sin(2*M_PI*(((x-g_min.x())/(Rg_x))*(Rd_x)+d_min.x()))*sin(2*M_PI*(((y-g_min.y())/(Rg_y))*(Rd_y)+d_min.y()))*sin(2*M_PI*(((z-g_min.z())/(Rg_z))*(Rd_z)+d_min.z())));
        //var[1]=(-12*M_PI*M_PI*sin(2*M_PI*(((x-g_min.x())/(Rg_x))*(Rd_x)+d_min.x()))*sin(2*M_PI*(((y-g_min.y())/(Rg_y))*(Rd_y)+d_min.y()))*sin(2*M_PI*(((z-g_min.z())/(Rg_z))*(Rd_z)+d_min.z())));
        //var[2]=(-12*M_PI*M_PI*sin(2*M_PI*(((x-g_min.x())/(Rg_x))*(Rd_x)+d_min.x()))*sin(2*M_PI*(((y-g_min.y())/(Rg_y))*(Rd_y)+d_min.y()))*sin(2*M_PI*(((z-g_min.z())/(Rg_z))*(Rd_z)+d_min.z())));
    };

    std::function<void(double,double,double,double*)> f_init =[d_min,d_max,g_min,g_max,Rg_x,Rg_y,Rg_z,Rd_x,Rd_y,Rd_z](const double x,const double y,const double z,double *var){
        var[0]=0;//(-12*M_PI*M_PI*sin(2*M_PI*(((x-g_min.x())/(Rg_x))*(Rd_x)+d_min.x()))*sin(2*M_PI*(((y-g_min.y())/(Rg_y))*(Rd_y)+d_min.y()))*sin(2*M_PI*(((z-g_min.z())/(Rg_z))*(Rd_z)+d_min.z())));
        //var[1]=0;
        //var[2]=0;
    };


    /*std::vector<ot::TreeNode> oct;
    createRegularOctree(oct,4,m_uiDim,m_uiMaxDepth,comm);
    ot::DA<dim>* octDA=new ot::DA<dim>(oct,comm,eOrder,2,0.2);*/

    ot::DA<dim>* octDA=new ot::DA<dim>(f_rhs,1,comm,eOrder,wavelet_tol,100,partition_tol);

    std::vector<double> uSolVec;
    octDA->createVector(uSolVec,false,false,DOF);
    double *uSolVecPtr=&(*(uSolVec.begin()));

    HeatEq::HeatMat<dim> heatMat(octDA,1);
    heatMat.setProblemDimensions(domain_min,domain_max);

    HeatEq::HeatVec<dim> heatVec(octDA,1);
    heatVec.setProblemDimensions(domain_min,domain_max);

    // TODO The DOF pointers are going to be wrong because
    //   variables are stored [abc][abc], not stored contiguously.
    //   Neet to correct the implementation of getVecPointerToDof()
    //   and then use the returned pointers with some stride.

    double * ux=octDA->getVecPointerToDof(uSolVecPtr,VAR::M_UI_U, false,false);
    double * frhs=octDA->getVecPointerToDof(uSolVecPtr,VAR::M_UI_F, false,false);
    double * Mfrhs=octDA->getVecPointerToDof(uSolVecPtr,VAR::M_UI_MF, false,false);


    octDA->setVectorByFunction(ux,f_init,false,false,1);
    octDA->setVectorByFunction(Mfrhs,f_init,false,false,1);
    octDA->setVectorByFunction(frhs,f_rhs,false,false,1);

    heatVec.computeVec(frhs,Mfrhs,1.0);


    double tol=1e-6;
    unsigned int max_iter=1000;
    heatMat.cgSolve(ux,Mfrhs,max_iter,tol,0);

    const char * vNames[]={"m_uiU","m_uiFrhs","m_uiMFrhs"};
    octDA->vecTopvtu(uSolVecPtr,"heatEq",(char**)vNames,false,false,DOF);
    octDA->destroyVector(uSolVec);

    if(!rank)
        std::cout<<" end of heatEq: "<<std::endl;

    delete octDA;

    MPI_Finalize();
    return 0;
}
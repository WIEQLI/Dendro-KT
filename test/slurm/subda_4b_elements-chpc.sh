#!/bin/bash
#SBATCH --time=8:00:00     # walltime, abbreviated by -t
#SBATCH -o out-job-%j-node-%N.out      # name of the stdout, using the job number (%j) and 
                                       # the first node (%N)
#SBATCH -e err-job-%j-node-%N.err      # name of the stderr, using job and first node values

#SBATCH --nodes=1          # request 1 node
#SBATCH --ntasks=20        # maximum total number of mpi tasks across all nodes.
#SBATCH -C "m384"          # a node with at least 384 GB of memory.

#SBATCH --job-name=DendroKT-4b
#SBATCH --mail-type=ALL
#SBATCH --mail-user=masado@cs.utah.edu

# additional information for allocated clusters
#SBATCH --account=soc-kp    # account - abbreviated by -A
#SBATCH --partition=soc-kp  # partition, abbreviated by -p


# load appropriate modules
module load intel impi

RUNPROGRAM=./../build/tstBillionsOfElements

NP=20

mpirun -np $NP ./$RUNPROGRAM

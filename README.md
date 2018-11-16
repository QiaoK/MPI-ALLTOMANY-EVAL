## Evaluate All-to-many Communication Using MPI Asynchronous Functions

This repository contains a program designed to evaluate the performance of
all-to-many personalized communication implemented using MPI asynchronous
communication functions `MPI_Isend`, `MPI_Irecv`, and `MPI_Waitall`.

The program `isendrecv.c` runs the all-to-many communication in a loop,
containing only calls to `MPI_Isend`, `MPI_Irecv`, and `MPI_Waitall`, but no
other MPI communication functions. The number of iterations can be set by the
command-line option `-n`. The default is 100. The message sizes are set to be
different among processes, but they are all multiples of the `unit message
size`. The default `unit message size` is 10 bytes, adjustable by command-line
option `-b`.

Program `isendrecv.c` is designed to run multiple compute nodes with multiple
MPI processes on each node. The command-line option `-p` should be set to the
number of MPI processes per compute node.

When running this program on
[Cori](http://www.nersc.gov/users/computational-systems/cori) KNL nodes with
64 MPI processes on each node, we found that the timing of posting `MPI_Irecv`
starts to become poor when the number of iterations is greater than 16.

* Compile command:
  * Edit file `Makefile` to customize the C compiler and compile options.
  * Run command `make` to compile and generate the executable program named
    `isendrecv`.
  * To compile for KNL nodes on Cori @NERSC, run command model swap below,
    before running `make`.
    ```
      % module swap craype-haswell craype-mic-knl
    ```
* Run command:
  * Example run command using `mpiexec` and 512 MPI processes:
    ```
      % mpiexec -n 512 ./isendrecv -p 64 -b 32 -n 20
    ```
  * Command-line options:
    ```
      % ./isendrecv -h
      Usage: isendrecv [OPTION]... [FILE]...
         [-h] Print help
         [-p] number of MPI processes per node
         [-b] message block unit size
         [-n] number of iterations
    ```
  * An example batch script file for job running on Cori @NERSC is given in
    `./slurm.knl`.

* Example outputs on screen
  ```
    % srun -n 512 -c 4 --cpu_bind=cores ./isendrecv -p 64 -b 32 -n 2
    -----------------------------------------------------------
    Total number of MPI processes        = 512
    Total number of sender   processes   = 512
    Total number of receiver processes   = 8
    Total number of iterations           = 2
    Message block unit size              = 32 bytes
    Max message size                     = 256 bytes
    Min message size                     = 32 bytes
    Max time of posting irecv            = 0.0117 sec
    Max time of posting isend            = 0.0041 sec
    Max time of MPI_Waitall()            = 0.0791 sec
    Max end-to-end time                  = 0.0829 sec
    (Max means the maximum among all 512 processes)
    -----------------------------------------------------------

    % srun -n 512 -c 4 --cpu_bind=cores ./isendrecv -p 64 -b 32 -n 8
    -----------------------------------------------------------
    Total number of MPI processes        = 512
    Total number of sender   processes   = 512
    Total number of receiver processes   = 8
    Total number of iterations           = 8
    Message block unit size              = 32 bytes
    Max message size                     = 256 bytes
    Min message size                     = 32 bytes
    Max time of posting irecv            = 0.0824 sec
    Max time of posting isend            = 0.0047 sec
    Max time of MPI_Waitall()            = 0.1161 sec
    Max end-to-end time                  = 0.1895 sec
    (Max means the maximum among all 512 processes)
    -----------------------------------------------------------

    % srun -n 512 -c 4 --cpu_bind=cores ./isendrecv -p 64 -b 32 -n 16
    -----------------------------------------------------------
    Total number of MPI processes        = 512
    Total number of sender   processes   = 512
    Total number of receiver processes   = 8
    Total number of iterations           = 16
    Message block unit size              = 32 bytes
    Max message size                     = 256 bytes
    Min message size                     = 32 bytes
    Max time of posting irecv            = 0.4630 sec
    Max time of posting isend            = 0.0044 sec
    Max time of MPI_Waitall()            = 0.2542 sec
    Max end-to-end time                  = 0.6427 sec
    (Max means the maximum among all 512 processes)
    -----------------------------------------------------------

    % srun -n 512 -c 4 --cpu_bind=cores ./isendrecv -p 64 -b 32 -n 32
    -----------------------------------------------------------
    Total number of MPI processes        = 512
    Total number of sender   processes   = 512
    Total number of receiver processes   = 8
    Total number of iterations           = 32
    Message block unit size              = 32 bytes
    Max message size                     = 256 bytes
    Min message size                     = 32 bytes
    Max time of posting irecv            = 5.6778 sec
    Max time of posting isend            = 0.0072 sec
    Max time of MPI_Waitall()            = 1.6124 sec
    Max end-to-end time                  = 6.2541 sec
    (Max means the maximum among all 512 processes)
    -----------------------------------------------------------
  ```

## Questions/Comments:
email: wkliao@eecs.northwestern.edu

Copyright (C) 2018, Northwestern University.

See [COPYRIGHT](COPYRIGHT) notice in top-level directory.


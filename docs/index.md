---
layout: default
title: Home
---

# ArgoDSM

ArgoDSM is a software distributed shared memory system.

## Quickstart Guide

ArgoDSM uses the CMake build system. In the next section, we will describe how
to build ArgoDSM from scratch. However, before doing that, some dependencies are
required.

The only distributed backend ArgoDSM currently supports is the MPI one, so a
compiler and libraries for MPI are required. If you are using OpenMPI, we
recommend using the latest stable release avaiable for your system.
Installing OpenMPI is
[fairly easy](https://www.open-mpi.org/faq/?category=building#easy-build), but
you should contact your system administrator if you are uncertain about it.

ArgoDSM depends on the
[C++ QD Locking Library](https://github.com/davidklaftenegger/qd_library).
This is included as a git submodule and is built automatically by CMake,
requiring both git and an internet connection. For instructions on how to build
offline, see [building ArgoDSM offline](#building-argodsm-offline).

Additionally, ArgoDSM requires `libnuma` to detect whether it is running on top
of NUMA systems, and if so how they are structured internally.

If you want to build the documentation,
[doxygen](https://www.doxygen.nl/) is required.

Finally, C and C++ compilers that support the C17 and C++17 standards
respectively are required.

### Building ArgoDSM

Note: adjust the below commands to your needs, especially `CMAKE_INSTALL_PREFIX`.

``` bash
git clone https://github.com/etascale/argodsm.git
cd argodsm && mkdir build && cd build
cmake -DARGO_BACKEND_MPI=ON              \
      -DARGO_BACKEND_SINGLENODE=ON       \
      -DARGO_TESTS=ON                    \
      -DBUILD_DOCUMENTATION=ON           \
      -DCMAKE_CXX_COMPILER=mpic++        \
      -DCMAKE_C_COMPILER=mpicc           \
      -DCMAKE_INSTALL_PREFIX=/usr/local/ \
      ../
make
make test
make install
```

Initially, you need to get the ArgoDSM sources. You can get the latest code from
the Github repository.

``` bash
git clone https://github.com/etascale/argodsm.git
```

CMake supports building in a separate folder. This is recommended for two
reasons. First of all, it makes cleaning up much easier. Second, it allows for
different builds with different configurations to exist in parallel.

``` bash
cd argodsm && mkdir build && cd build
```

In order to generate the makefiles with CMake, you can use either the `cmake` or
the `ccmake` tool. The difference is that the first one accepts all the build
options as command line arguments, while the second one works interactively.
Below is an example call to `cmake` with all the recommended command line
arguments. If you plan on contributing to the ArgoDSM source code, you should
also enable the `ARGO_DEBUG` option. Remember to change `CMAKE_INSTALL_PREFIX`
to a path that you have write access to. After generating the makefiles proceed
to building the library and executables with a simple make command.

If you are planning on building the ArgoDSM tests (recommended), CMake will
automatically include the [googletest](https://github.com/google/googletest/)
framework as a submodule and build this as part of the project. To disable
building googletest and the ArgoDSM test suite, set `ARGO_TESTS=OFF`.

``` bash
cmake -DARGO_BACKEND_MPI=ON              \
      -DARGO_BACKEND_SINGLENODE=ON       \
      -DARGO_TESTS=ON                    \
      -DBUILD_DOCUMENTATION=ON           \
      -DCMAKE_CXX_COMPILER=mpic++        \
      -DCMAKE_C_COMPILER=mpicc           \
      -DCMAKE_INSTALL_PREFIX=/usr/local/ \
      ../
make
```

After the build process has finished, all the executables can be found in the
`bin` directory. It will contain different subdirectories, one for each backend.
It is **highly recommended** to run the test suite before proceeding, to ensure
that everything is working fine.

If you want to build and manually compile your own applications, you can find
the libraries in the `lib` directory. You need to link with the main `libargo`
library, as well as *exactly* one of the backend libraries.

``` bash
make test
```

This step executes the ArgoDSM tests. Tests for the MPI backend are run on two
ArgoDSM software nodes by default. This number can be changed through setting
the `ARGO_TESTS_NPROCS` CMake option to any number from 1 to 8. Keep in mind
that some MPI distributions may not allow executing more processes than the
number of physical cores in the system. Please note that some issues are only
encountered when running with more than two ArgoDSM nodes or on multiple
hardware nodes. For this reason it is highly recommended to run the MPI tests
on at least four hardware nodes when possible. Refer to the next section to
learn how to run applications on multiple hardware nodes.

``` bash
make install
```

This step will copy the final ArgoDSM include files to `/usr/local/include` and
libraries to `/usr/local/lib/`. You can choose a different path above if you
want, but remember to either set `LIBRARY_PATH`, `INCLUDE_PATH`, and
`LD_LIBRARY_PATH` accordingly, or provide the correct paths (`-L`, `-I`, and
`-Wl,-rpath,`) to your compiler when compiling applications for ArgoDSM.

### Running the Applications

For the singlenode backend, the executables can be run like any other normal
executable.

For the MPI backend, they need to be run using the matching `mpirun`
application. You should refer to your MPI's vendor documentation for more
details. If you are using OpenMPI on a cluster with InfiniBand interconnects, we
recommend the following:

``` bash
mpirun --map-by ppr:1:node  \
       --mca pml ucx        \
       --mca osc ucx        \
       ${EXECUTABLE}
```

The number of nodes is controlled by your job scheduling system or by supplying
`mpirun` with a nodelist, and `${EXECUTABLE}` is the application to run.

If you are running on a cluster, please consult your system administrator for
details on how to access the cluster's resources. We recommend running ArgoDSM
applications with one ArgoDSM node per hardware node. We also strongly recommend
InfiniBand interconnects when using the MPI backend. If you are not using
InfiniBand, you can use the default OpenMPI transports by excluding the `--mca`
parameters.

If you are not running on a cluster, you can instead specify the number of
ArgoDSM nodes to run by passing `-n ${NNODES}` to mpirun as:

``` bash
mpirun -n ${NNODES} ${EXECUTABLE}
```

We **strongly recommend** running all of the MPI tests on at least two nodes
before continueing working with ArgoDSM, to ensure that your system is
configured properly and also that it is fully supported by ArgoDSM.

### Building ArgoDSM offline
If you are building ArgoDSM in an **offline** environment, submodules will first
have to be updated in an online environment. This can be done either by running
CMake, manually executing `git submodule update --init --recursive` or using
`git clone --recurse-submodules https://github.com/etascale/argodsm.git` to clone
the ArgoDSM repository. Once submodules are updated, your ArgoDSM directory can
safely be moved to the offline environment. Finally, before building offline,
set `GIT_SUBMODULE=OFF` in CMake.

## Contributing to ArgoDSM

If you are interested in contributing to ArgoDSM, do not hesitate to contact us.
Please make sure that you understand the license terms.

### Code Style

See [Code Style](code-style.html)

### Code Review Process

See [Code Style](code-style.html)

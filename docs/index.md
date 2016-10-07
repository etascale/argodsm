---
pagetitle: Home
navbar: true
---

# ArgoDSM

ArgoDSM is a software distributed shared memory system.

## Quickstart Guide

ArgoDSM uses the CMake build system. In the next section, we will describe how
to build ArgoDSM from scratch. However, before doing that, some dependencies are
required.

If you want to build the documentation,
[doxygen](http://www.stack.nl/~dimitri/doxygen/) is required.

The only distributed backend ArgoDSM currently supports is the MPI one, so a
compiler and libraries for MPI are required. If you are using OpenMPI, make sure
you are running at least version 1.8.4. Installing OpenMPI is [fairly
easy](https://www.open-mpi.org/faq/?category=building#easy-build), but you
should contact your system administrator if you are uncertain about it.

Additionally, ArgoDSM requires `libnuma` to detect whether it is running on top
of NUMA systems, and if so how they are structured internally.

Finally, C and C++ compilers that support the C11 and C++11 standards
respectively are required.

### Building ArgoDSM

~~~{.Bash}
git clone https://github.com/etascale/argodsm.git
cd argodsm
cd tests
wget https://github.com/google/googletest/archive/release-1.7.0.zip
unzip release-1.7.0.zip
mv googletest-release-1.7.0 gtest-1.7.0
cd ..
mkdir build
cd build
cmake -DARGO_BACKEND_MPI=ON        \
      -DARGO_BACKEND_SINGLENODE=ON \
      -DARGO_TESTS=ON              \
      -DBUILD_DOCUMENTATION=ON     \
      -DCMAKE_CXX_COMPILER=mpic++  \
      -DCMAKE_C_COMPILER=mpicc     \
      ../
make
make test
~~~

Initially, you need to get the ArgoDSM sources. If you already have a tarball,
you can skip this step and just extract its contents. Otherwise, if you have
access to it, you can get the latest code from the Github repository.

~~~{.Bash}
git clone https://github.com/etascale/argodsm.git
cd argodsm
~~~

If you are planning on building the ArgoDSM tests (recommended), you also need
the [googletest](https://github.com/google/googletest/) framework. Extract it
into the `tests` folder, and make sure that its folder is named `gtest-1.7.0`.

~~~{.Bash}
cd tests
wget https://github.com/google/googletest/archive/release-1.7.0.zip
unzip release-1.7.0.zip
mv googletest-release-1.7.0 gtest-1.7.0
cd ..
~~~

CMake supports building in a separate folder. This is recommended for two
reasons. First of all, it makes cleaning up much easier. Second, it allows for
different builds with different configurations to exist in parallel.

~~~{.Bash}
mkdir build
cd build
~~~

In order to generate the makefiles with CMake, you can use either the `cmake` or
the `ccmake` tool. The difference is that the first one accepts all the build
options as command line arguments, while the second one works interactively.
Below is an example call to `cmake` with all the recommended command line
arguments. If you plan on contributing to the ArgoDSM source code, you should
also enable the `ARGO_DEBUG` option. After generating the makefiles, then just
build the library and executables with a simple make command.

~~~{.Bash}
cmake -DARGO_BACKEND_MPI=ON        \
      -DARGO_BACKEND_SINGLENODE=ON \
      -DARGO_TESTS=ON              \
      -DBUILD_DOCUMENTATION=ON     \
      -DCMAKE_CXX_COMPILER=mpic++  \
      -DCMAKE_C_COMPILER=mpicc     \
      ../
make
~~~

After the build process has finished, all the executables can be found in the
`bin` directory. It will contain different subdirectories, one for each backend.
It is **highly recommended** to run the test suite before proceeding, to ensure
that everything is working fine.

If you want to build and manually compile your own applications, you can find
the libraries in the `lib` directory. You need to link with the main `libargo`
library, as well as *exactly* one of the backend libraries.

~~~{.Bash}
make test
~~~

Keep in mind that currently even the MPI tests are run on a single node without
invoking `mpirun`. After running the previous command, you should also run the
tests with at least 4 nodes. To learn how to run the MPI tests on multiple
nodes, proceed to the next section.

### Running the Applications

For the singlenode backend, the executables can be run like any other normal
executable.

For the MPI backend, they need to be run using the matching `mpirun`
application. You should refer to your MPI's vendor documentation for more
details. If you are using OpenMPI on a cluster with InfiniBand interconnects, we
recommend the following:

~~~{.Bash}
mpirun --map-by ppr:1:node      \
       --mca mpi_leave_pinned 1 \
       --mca btl openib,self,sm \
       -n ${NNODES}             \
       ${EXECUTABLE}
~~~

`${NNODES}` is the number of hardware nodes you have available and
`${EXECUTABLE}` is the application to run. If you are not using InfiniBand, you
can use the default OpenMPI transports by skipping the `--mca btl
openib,self,sm` part  of the command.

If you are running on a cluster, please consult your system administrator for
details on how to access the cluster's resources. We recommend running ArgoDSM
applications with one ArgoDSM node per hardware node. We also strongly recommend
InfiniBand interconnects when using the MPI backend.

We **strongly recommend** running all of the MPI tests on at least two nodes
before continueing working with ArgoDSM, to ensure that your system is
configured properly and also that it is fully supported by ArgoDSM.

## Contributing to ArgoDSM

If you are interested in contributing to ArgoDSM, do not hesitate to contact us.
Please make sure that you understand the license terms.

### Code Style

See [Code Style](code-style.html)

### Code Review Process

See [Code Style](code-style.html)


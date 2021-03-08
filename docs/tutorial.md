---
layout: default
title: Tutorial
---

Tutorial
========

This is a set of small tutorials on how to convert pthreads applications to run
on ArgoDSM. We will start with a very simple example, and then move to real
pthreads applications.

We assume that you have already [compiled the ArgoDSM libraries]({{
site.baseurl}}) and
that you have installed them in a user local directory. We will also assume
knowledge of the Pthreads library and programming model, as well as very basic
knowledge of MPI. If you are not familiar with either, you might find this
tutorial confusing.

## The Simple Example

This example is meant as an introduction to ArgoDSM and its differences with
non-distributed pthreads applications. It is written in C++. For brevity,
we assume that the input data is evenly divisible by the number of threads
and that the number of threads is evenly divisible by the number of nodes. We
also omit error checking code.

### Just Pthreads

```cpp
{% include pthreads_example.cpp %}
```

The example finds the biggest number in an integer array. The astute reader
might realize that the biggest number is actually always the same and it can be
determined by a very simple calculation, but this is after all just an example.
The array is initialized by the main thread and then the worker threads are
started. Each worker threads receives as an argument a part of the array, in the
form of \[first, last) index. While the worker threads are working, the main
thread is waiting for them to finish. After they are done, the main thread
accesses the `max` variable, prints it, and makes sure the result is correct.

The worker threads work only on their local part of the array. After they find
the local maximum, then they check and update the global maximum (`max`)
variable. Since multiple threads might access this variable at the same time, it
is protected by locks.

### Scaling it up with ArgoDSM

The first step is adding the ArgoDSM initialization and finalization function.
Specifically, `argo::init` needs to be run in `main` before any other functions
are called, and `argo::finalize` should be the last ArgoDSM function called
before exiting `main`. In the current version of ArgoDSM, `argo::init` has two
optional arguments which indicate the amount of shared memory (in bytes) ArgoDSM
should allocate and the desired local cache size, respectively. When the values
are given to `argo::init`, they will be used. If one or both are omitted, then
the value (in bytes) in the environment variable `ARGO_MEMORY_SIZE` (for the
shared memory size) and `ARGO_CACHE_SIZE` (for the local cache size) will be
used instead. If the environment variables are empty, some default numbers will
be used. At the time of this writing they are 8GB and 1GB for shared memory and
local cache size, respectively.

In addition to adding the ArgoDSM initialization and finalization functions,
there are three main tasks that need to be done to convert a pthreads
application to ArgoDSM.

1. Remove data races.
2. Allocate shared (ArgoDSM) data using the ArgoDSM allocators.
3. Replace any Pthreads synchronization primitives with corresponsing primitives
   provided by ArgoDSM.
4. Take care of the changes introduced by the new multiple process model.

Removing data races (1) is unnecessary in this case, since the pthreads
application is standard (both POSIX and C++) compliant, so it is already data
race free.

In order to allocate all the shared data in the global memory (2), we need to
make two changes. First, we need to switch any calls to `new` with calls to the
ArgoDSM allocators. Specifically, we need to replace `new int[data_length]` with
`argo::conew_array<int>(data_length)`. This allocation function
`argo::conew_array` is run on all the nodes, returning the same
pointer to all of them, thus initializing the `data` variable in all of them. So
this:

``` cpp
data = new int[data_length];
```

needs to become this:

``` cpp
data = argo::conew_array<int>(data_length);
```

Then, we need to move the `max` variable on the shared memory. ArgoDSM currently
does not support mapping statically allocated variables into the global memory,
so we will convert `max` into a pointer and allocate memory for it on the global
memory. We will use the `argo::conew_<int>` function for this, giving as an
initialization argument the same argument as we have in the pthreads example.
This means changing this:

``` cpp
int max = std::numeric_limits<int>::min();
```

to this:

``` cpp
int *max;
... // Somewhere inside main
max = argo::conew_<int>(std::numeric_limits<int>::min());
```

Note that the ArgoDSM functions can only be used after `argo::init` has been
called, so we need to move the allocation and initialization of `max` inside the
`main` function.

In addition to the `argo::conew_` family of functions, which needs to be called
by **all** the nodes at the same time, there is also the `argo::new_`
family which behaves like normal `new` but allocates globally shared memory.

We can now move on to the synchronization primitives(3). For this example we
will use the `argo::globallock::global_tas_lock` available in ArgoDSM,
but the `argo::simple_lock`, which will be made available soon, should be used
instead. The `global_tas_lock` requires as an argument a boolean variable to
spin on, which of course has to be allocated on the global memory.

``` cpp
lock_field = argo::conew_<argo::globallock::global_tas_lock::internal_field_type>();
lock = new argo::globallock::global_tas_lock(lock_field);
```

Finally, we need to take into consideration the fact that ArgoDSM runs multiple
processes (which also means multiple threads) from the beginning of the program
execution (4). Essentially, each node has one process which has one main thread.
For each of those processes, we want to initialize the *non-shared* global data
individually (this is why we are using `argo::conew_`) but at the same time we
want **only one** node to initialize the *shared* global data. If multiple nodes
where to initialize the same data at the same time, we would have a data race.
Usually, the simplest way to do that is to have `Node 0` initialize all the data
and then synchronize with all the other nodes. We achieve that by using an `if`
statement in conjunction with `argo::barrier`. The same goes for any work done
by the application after the threads have finished; we want to make sure that
work that needs to be done by only one thread will be done by only one node.

``` cpp
if (argo::node_id() == 0) {
	for (int i = 0; i < data_length; ++i)
		data[i] = i * 11 + 3;
}
argo::barrier();
```

We also need to remember that each node starts its own threads, so we should
either change how many threads each node starts or how we partition the shared
data. In this example we decided to simple evenly split the number of threads
between the available nodes.

``` cpp
local_num_threads = num_threads / argo::number_of_nodes();
...
int chunk = data_length / num_threads;
std::vector<pthread_t> threads(local_num_threads);
std::vector<thread_args> args(local_num_threads);
for (int i = 0; i < local_num_threads; ++i) {
	int global_tid = (argo::node_id() * local_num_threads) + i;
	args[i].data_begin = global_tid * chunk;
	args[i].data_end = args[i].data_begin + chunk;
	pthread_create(&threads[i], nullptr, parmax, &args[i]);
}
```

The final result of all the changes is this:

``` cpp
{% include argo_example.cpp %}
```

We can now compile the application. We assume that `${ARGO_INSTALL_DIRECTORY}`
is where you installed ArgoDSM. If the ArgoDSM files are already in your
`LIBRARY_PATH`, `INCLUDE_PATH`, and `LD_LIBRARY_PATH`, you can skip the
`-L...`, `-I...`, and `-Wl,-rpath,...` switches. If you have no idea what we
are talking about, you should ask your system administrator.

``` bash
mpic++ -O3 -std=c++11 -o argo_example            \
       -L${ARGO_INSTALL_DIRECTORY}/lib           \
       -I${ARGO_INSTALL_DIRECTORY}/include       \
       -Wl,-rpath,${ARGO_INSTALL_DIRECTORY}/lib  \
       argo_example.cpp -largo -largobackend-mpi
```

This should produce an executable file that can be run with MPI. For
instructions on how to run MPI applications you should contact your local
support office. A generic example with OpenMPI on a cluster utilizing Slurm
might look like this:

``` bash
mpirun --map-by ppr:1:node                                               \
	--mca mpi_leave_pinned 1 --mca btl openib,self,sm -n ${SLURM_NNODES} \
	./argo_example
```

### Optimizing data placement for performance

ArgoDSM has a variety of page placement policies to choose from, in order to
distribute the globally allocated data across the different nodes of a cluster
machine. Currently, the total number of memory policies is five, namely `naive`,
`cyclic`, `skew-mapp`, `prime-mapp`, and `first-touch`. The default memory
management scheme is `naive`. The `naive` policy will use all available memory
(physical) contributed to the global address space from the first node, before
using the next node's memory. Running a memory intensive program under this
memory policy, it is a good practice for the Argo global memory size to match
the size of the globally allocated data, in order for pages to be equally
distributed across nodes and avoid bottlenecks.

The cyclic group of memory policies which consists of `cyclic`, `skew-mapp`, and
`prime-mapp`, spreads memory pages over the memory modules of a cluster machine
following a type of round-robin distribution, thus balancing memory modules'
usage, improving network bandwidth, and easing programmability, since whatever
size is provided to the initializer call `argo::init` does not affect the
placement of data. The `cyclic` policy distributes data in a linear way, meaning
that it uses a block of pages per round and places it in the memory module
`b mod N`, where `N` is the number of nodes used to run the application. Since
this linear distribution can still lead to contention problems in some scientific
applications, the `skew-mapp` and `prime-mapp` policies are introduced. These two
allocation schemes perform a non-linear page placement over the machines memory
modules, in order to reduce concurrent accesses directed to the same memory
modules. The `skew-mapp` policy is a modification of the `cyclic` policy that has
a liner page skew. It is able to skip a page for every `N` (number of nodes)
pages allocated, resulting in a non-uniform distribution of pages across the
memory modules of the distributed system. The `prime-mapp` memory policy uses a
two-phase round-robin strategy to better distribute pages over a cluster machine.
In the first phase, the policy places data using the `cyclic` policy in `P` nodes,
where `P` is a number greater than or equal to `N` (number of nodes), and is equal
to `3N/2`. In the second phase, the memory pages previously placed in the virtual
nodes are re-placed into the memory modules of the real nodes also using the
`cyclic` policy. In this way, the memory modules of the real nodes are not used in
a uniform way to place memory pages.

The `first-touch` policy places data in the memory module of the node that first
accesses it. Due to this characteristic, data initialization must be done with care
so that data is first accessed by the process that is later on going to use it.
That said, to achieve optimal performance running the `argo_example` under the
`first-touch` memory policy, team process initialization instead of master process
initialization should be used, as being seen below. The `beg` and `end` variables
are calculated based on the `id` of each process and define the chunk in `data`
that each process will initialize.

``` cpp
int chunk = data_length / argo::number_of_nodes();
int beg = argo::node_id() * chunk;
int end = beg + chunk;
for (int i = beg; i < end; ++i)
	data[i] = i * 11 + 3;
argo::barrier();
```

The memory allocation policy can be selected through the environment variable
`ARGO_ALLOCATION_POLICY`, which should be given a number from `0` to `4`,
starting from the default `naive` distribution and continuing with the `cyclic`,
`skew-mapp`, `prime-mapp`, and `first-touch` policies, as shown in the table
below. The page block size when utilizing one of the cyclic policies can be
adjusted by setting `ARGO_ALLOCATION_BLOCK_SIZE`. In case the former environment
variable is not specified, the `naive` distribution will be used and, in the
case of the latter, a size of `16` will be selected, resulting in a granularity
of 16*4KB=64KB.

When running under `first-touch` or under any of the `cyclic` policies with a
small granularity, you may need to increase `vm.max_map_count` significantly
above the default (65536). If you don't know how to do this, contact your system
administrator.

| Memory Policy | ARGO_ALLOCATION_POLICY | ARGO_ALLOCATION_BLOCK_SIZE |
|:-------------:|:----------------------:|:--------------------------:|
| naive         |       0 (default)      |              -             |
| cyclic        |            1           |        16 (default)        |
| skew-mapp     |            2           |        16 (default)        |
| prime-mapp    |            3           |        16 (default)        |
| first-touch   |            4           |              -             |
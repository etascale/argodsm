---
layout: default
title: Advanced
---

Advanced Topics
===============

This document contains some information about the more advanced aspects of
ArgoDSM.

- [Allocation Parameters](#allocation-parameters)
- [Virtual Memory Management](#virtual-memory-management)
- [Data Allocation Policies](#data-allocation-policies)
- [Remote Page Loading and Prefetching](#remote-page-loading-and-prefetching)
- [Write-buffer Tuning](#write-buffer-tuning)
- [Multi-threaded MPI Support](#multi-threaded-mpi-support)
- [ArgoDSM statistics](#argodsm-statistics)


## Allocation Parameters

All the allocation functions available in ArgoDSM are function templates that
accept three sets of template parameters.

1. The type to be allocated (required)
2. Allocation parameters (optional)
3. Constructor argument types (optional)

The first set consists of exactly one parameter, which is the type to allocate
and potentially initialize. The second set consists of values of type
`argo::allocation`, which allow the user to alter the behavior of the allocation
function. The third set consists of the types of the constructor arguments, and
it's best left blank, as the compiler can deduce it automatically.

Other than the template parameters, the allocation functions also accept
constructor arguments, when applicable. Specifically, the single object
allocation functions accept constructor arguments, while the array allocation
functions do not. Instead, they accept the size (in elements) of the array. This
resembles the C++ `new` expressions, where it is not possible to initialize a
whole array with a single value.

As mentioned earlier, the allocation functions can also initialize the allocated
space, based on the type allocated. This is done for both the single and the
array allocations, but not for all cases. Specifically, if the allocated type is
of [`TrivialType`](http://en.cppreference.com/w/cpp/concept/TrivialType) then it
will not be initialized, unless constructor arguments are provided. For example:

``` cpp
int *a = argo::new_<int>();  // Not initialized
int *b = argo::new_<int>(0); // Initialized
```

In addition to the initialization of the allocated memory, the collective
allocation functions can also act as a synchronization mechanism between the
different threads. This is achieved through the `argo::barrier` function, and
the synchronization is implied if initialization also takes place. In the case
of the dynamic allocation functions, synchronization is never performed.

Which brings us to the allocation parameters mentioned above. It is possible for
the user to alter the behavior of the allocation functions by passing the
appropriate allocation parameters.
It is possible to explicitly enable or disable both the initialization and the
synchronization that each function might perform, with the exception that the
dynamic allocation functions ignore the synchronization arguments. For example:

``` cpp
int *a = argo::new_<int, argo::allocation::initialize>();     // Initialized
int *b = argo::new_<int, argo::allocation::no_initialize>(0); // Not initialized
```

The deallocation functions work similarly to their allocation counterparts, only
some of the allocation parameters are named differently, as instead of
initialization, destruction is performed. So, instead of accepting
`argo::[no_]initialize` they accept `argo::[no_]deinitialize`. Also, only one
function argument is accepted, and that is the pointer to the memory that will
be deallocated.


## Virtual Memory Management

To manage the virtual address space for ArgoDSM applications, we acquire large
amounts of virtual memory. There are currently three ways to acquire the
underlying memory for ArgoDSM, and exactly one must be selected at compile time:

1. POSIX shared memory objects (`-DARGO_VM_SHM`)
2. `memfd_create` syscall      (`-DARGO_VM_MEMFD`)
3. anonymous remappable memory (`-DARGO_VM_ANONYMOUS`)

Each version has its own downsides:
1. POSIX shared memory objects are size-limited and require `/dev/shm` support.
2. `memfd_create` requires Linux kernel version of 3.17+ and memory overcommit.
3. anonymous remappable memory has a runtime overhead, and relies on kernel
   functionality that is deprecated since Linux version 3.16.

For now, the default is to use POSIX shared memory objects.


## Data Allocation Policies

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

The cyclic group of memory policies, which consists of `cyclic`, `skew-mapp`, and
`prime-mapp`, spreads memory pages over the memory modules of a cluster machine
following a type of round-robin distribution, thus balancing memory modules'
usage, improving network bandwidth, and easing programmability, since whatever
size is provided to the initializer call `argo::init` does not affect the
placement of data. The `cyclic` policy distributes data in a linear way, meaning
that it uses a block of pages per round and places it in the memory module
`b mod N`, where `N` is the number of nodes used to run the application. Since
this linear distribution can still lead to contention problems in some scientific
applications, the `skew-mapp` and `prime-mapp` policies were introduced. These two
allocation schemes perform a non-linear page placement over the machine's memory
modules, in order to reduce concurrent accesses directed to the same memory
modules. The `skew-mapp` policy is a modification of the `cyclic` policy that has
a liner page skew. It is able to skip a page for every `N` (number of nodes)
pages allocated, resulting in a non-uniform distribution of pages across the
memory modules of the distributed system. The `prime-mapp` memory policy uses a
two-phase round-robin strategy to better distribute pages over a cluster machine.
In the first phase, the policy places data using the `cyclic` policy in `P` nodes,
where `P` is a number greater than or equal to `N` (number of nodes), and is equal
to `3N/2`. In the second phase, the memory pages previously placed in the virtual
nodes are re-placed into the memory modules of the real nodes, also using the
`cyclic` policy. In this way, the memory modules of the real nodes are not used in
a uniform way to place memory pages.

The `first-touch` policy places data in the memory module of the node that first
accesses it. Due to this characteristic, data initialization must be done with care
so that data is first accessed by the process that is later on going to use it.
That said, to achieve optimal performance running the `argo_example` under the
`first-touch` memory policy, team process initialization instead of master process
initialization should be used, as we will see below. The `beg` and `end` variables
are calculated based on the `id` of each process, and define the chunk in `data`
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
of 16\*4KB=64KB.

When running under `first-touch` or under any of the `cyclic` policies with a
small granularity, one may need to increase `vm.max_map_count` significantly
above the default (65536). If you don't know how to do this, contact your system
administrator.

| Memory Policy | ARGO_ALLOCATION_POLICY | ARGO_ALLOCATION_BLOCK_SIZE |
|:-------------:|:----------------------:|:--------------------------:|
| naive         |       0 (default)      |              -             |
| cyclic        |            1           |        16 (default)        |
| skew-mapp     |            2           |        16 (default)        |
| prime-mapp    |            3           |        16 (default)        |
| first-touch   |            4           |              -             |


## Remote Page Loading and Prefetching

When a memory access misses in the ArgoDSM cache, the missing page has to be
loaded from a remote ArgoDSM node. Since remote operations are expensive in
terms of latency, ArgoDSM attempts to reduce the amount of cache misses by
fetching multiple contiguous pages on each remote load. This naïve type of
prefetching uses a significantly reduced amount of remote operations per page
fetched compared to loading pages one by one. The default number of pages
fetched on each remote load is `8`. If a specific application favours a
different number of pages fetched on each remote load, this number can be set
through the environment variable `ARGO_LOAD_SIZE` to any number greater
than `1`.

```sh
export ARGO_LOAD_SIZE=1		# Each remote load fetches one page
export ARGO_LOAD_SIZE=16	# Each remote load fetches up to 16 pages
```

By altering the load size to reflect the spatial locality found in each
specific application, it is possible to significantly reduce the amount
of cache misses, and consequently the number of remote operations
performed.


## Write-buffer Tuning

ArgoDSM uses a write buffer on each node that keeps track of which cached
pages are dirty and need to be written back upon synchronization. When
attempting to add a page to an already full write buffer, dirty pages at the
front of the write buffer are immediately written back to their home nodes
before the new page is added to the write buffer.

The default size of the write buffer is `512` pages. This is a good start
for most applications, but can be altered by setting the environment
variable `ARGO_WRITE_BUFFER_SIZE` to the desired number.

```sh
export ARGO_WRITE_BUFFER_SIZE=256
```

The number of pages written back on attempting to add a new page to a full
write buffer is by default `32`. If an application consistently fills up
the write buffer in between synchronization points, increasing this number
may be beneficial. This can be done by setting the environment variable
`ARGO_WRITE_BUFFER_WRITE_BACK_SIZE` to the desired number of pages.

```sh
export ARGO_WRITE_BUFFER_WRITE_BACK_SIZE=64
```


## Multi-threaded MPI Support

ArgoDSM now requires and utilizes multi-threaded MPI. Please ensure that
your MPI installation supports `MPI_THREAD_MULTIPLE`.

ArgoDSM uses multiple MPI windows in order to achieve parallelism between
simultaneous MPI operations. Greater possible parallelism can be achieved
by increasing the amount of MPI windows used, but creating too many windows
incurs an initialization and finalization time penalty. The number of MPI
windows used can be controlled by setting the `ARGO_MPI_WINDOWS_PER_NODE`
environment variable to the desired setting.

```sh
export ARGO_MPI_WINDOWS_PER_NODE=8
```

The default value of this environment variable is `4` when multiple
ArgoDSM nodes are used, and `1` in a single node setting. Note that the
total number of MPI windows created depends on the number of ArgoDSM nodes
(MPI processes) for a total of `ARGO_MPI_WINDOWS_PER_NODE*$NNODES*$NNODES*2`
MPI windows created per node. Your system may impose a hard limit on the
number of MPI windows created, and while the default value should be suitable
for most systems, if you experience significant startup times or crashes
during startup, consider lowering the number of MPI windows per node.


## ArgoDSM Statistics

ArgoDSM prints basic statistics after shutting down in order to give the
user an idea of how ArgoDSM behaves and to identify areas of improvement in
ArgoDSM utilization and tuning. The detail of the statistics printed can be
controlled by the `ARGO_PRINT_STATISTICS` environment variable as follows:

```sh
export ARGO_PRINT_STATISTICS=3
```

The different options available are detailed below.
- 0: Disable the statistics printout
- 1: Print overall system statistics
- 2: In addition to 1, print basic statistics for each ArgoDSM node
- 3: In addition to 2, print detailed statistics for each ArgoDSM node

Finally, it may be convenient to record statistics from a certain point
in the user code, such as after data initialization. To achieve this, the
ArgoDSM statistics can be reset by calling `argo::backend::reset_stats()`.

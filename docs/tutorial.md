---
pagetitle: Tutorial
navbar: true
---

Tutorial
========

This is a set of small tutorials on how to convert pthreads applications to run
on ArgoDSM. We will start with a very simple example, and then move to real
pthreads applications.

We assume that you have already [compiled the ArgoDSM libraries](/argodsm/) and
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

~~~{.Cpp .numberLines include="pthreads_example.cpp"}
~~~

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
before exiting `main`. In the current version of ArgoDSM, `argo::init` has one
required argument which indicates the total amount of memory ArgoDSM should
allocate. This will be deprecated in future versions.

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

~~~{.Cpp}
data = new int[data_length];
~~~

needs to become this:

~~~{.Cpp}
data = argo::conew_array<int>(data_length);
~~~

Then, we need to move the `max` variable on the shared memory. ArgoDSM currently
does not support mapping statically allocated variables into the global memory,
so we will convert `max` into a pointer and allocate memory for it on the global
memory. We will use the `argo::conew_<int>` function for this, giving as an
initialization argument the same argument as we have in the pthreads example.
This means changing this:

~~~{.Cpp}
int max = std::numeric_limits<int>::min();
~~~

to this:

~~~{.Cpp}
int *max;
... // Somewhere inside main
max = argo::conew_<int>(std::numeric_limits<int>::min());
~~~

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

~~~{.Cpp}
lock_flag = argo::conew_<bool>(false);
lock = new argo::globallock::global_tas_lock(lock_flag);
~~~

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

~~~{.Cpp}
if (argo::node_id() == 0) {
	for (int i = 0; i < data_length; ++i)
		data[i] = i * 11 + 3;
}
argo::barrier();
~~~

We also need to remember that each node starts its own threads, so we should
either change how many threads each node starts or how we partition the shared
data. In this example we decided to simple evenly split the number of threads
between the available nodes.

~~~{.Cpp}
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
~~~

The final result of all the changes is this:

~~~{.Cpp .numberLines include="argo_example.cpp"}
~~~

We can now compile the application. We assume that `${ARGO_INSTALL_DIRECTORY}` is
where you installed ArgoDSM. If the ArgoDSM files are already in your
`(LD_)LIBRARY_PATH` and include path, you can skip the `-L...` and `-I...`
switches. If you have no idea what we are talking about, you should ask your
system administrator.

~~~{.Bash}
mpic++ -O3 -std=c++11 -o argo_example   \
	-L${ARGO_INSTALL_DIRECTORY}/lib     \
	-I${ARGO_INSTALL_DIRECTORY}/include \
	argo_example.cpp -largo -largobackend-mpi
~~~

This should produce an executable file that can be run with MPI. For
instructions on how to run MPI applications you should contact your local
support office. A generic example with OpenMPI on a cluster utilizing Slurm
might look like this:

~~~{.Bash}
mpirun --map-by ppr:1:node                                               \
	--mca mpi_leave_pinned 1 --mca btl openib,self,sm -n ${SLURM_NNODES} \
	./argo_example
~~~

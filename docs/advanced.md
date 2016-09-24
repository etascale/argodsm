---
pagetitle: Advanced
navbar: true
---

Advanced Topics
===============

This document contains some information about the more advanced aspects of
ArgoDSM.

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

```{.Cpp}
int *a = argo::new_<int>(); // Not initialized
int *b = argo::new_<int>(0); // Initialized
```

In addition to the initialization of the allocated memory, the collective
allocation functions can also act as a synchronization mechanism between the
different threads. This is achieved through the `argo::barrier` function, and
the synchronization is implied if initialization also takes place. In the case
of the dynamic allocation functions, synchronization is never performed.

Which brings us to the allocation parameters mentioned above. It is possible for
the user to alter the behavior of the allocation functions by passing the
appropriate allocation parameters. At the time that this is being written, it is
possible to explicitly enable or disable both the initialization and the
synchronization that each function might perform, with the exception that the
dynamic allocation functions ignore the synchronization arguments. For example:

```{.Cpp}
int *a = argo::new_<int, argo::allocation::initialize>(); // Initialized
int *b = argo::new_<int, argo::allocation::no_initialize>(0); // Not initialized
```

The deallocation functions work similarly to their allocation counterparts, only
some of the allocation parameters are named differently, as instead of
initialization, destruction is performed. So, instead of accepting
`argo::[no_]initialize` they accept `argo::[no_]deinitialize`. Also, only one
function argument is accepted, and that is the pointer to the memory that will
be deallocated.


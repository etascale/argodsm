#include <cassert>
#include <limits>
#include <iostream>
#include <vector>

#include <pthread.h>

#include "argo/argo.hpp"

struct thread_args {
	int data_begin;
	int data_end;
};

int *data;
int *max;

bool *lock_flag;
argo::globallock::global_tas_lock *lock;

void* parmax(void* argptr) {
	// Get the arguments
	thread_args* args = static_cast<thread_args*>(argptr);

	// Let's declutter the code a bit, while also avoiding unnecessary global
	// memory accesses
	int data_begin = args->data_begin;
	int data_end = args->data_end;

	// Find the local maximum
	int local_max = std::numeric_limits<int>::min();
	for (int i = data_begin; i < data_end; ++i) {
		if (data[i] > local_max) {
			local_max = data[i];
		}
	}

	// Change the global maximum (if necessary)
	lock->lock();
	if (local_max > *max)
		*max = local_max;
	lock->unlock();

	return nullptr;
}

int main(int argc, char* argv[]) {
	int data_length = 160000;
	int num_threads = 16;
	int local_num_threads;

	// We totally need 10GB for this application
	argo::init(10*1024*1024*1024UL);

	local_num_threads = num_threads / argo::number_of_nodes();

	// Initialize the lock
	lock_flag = argo::conew_<bool>(false);
	lock = new argo::globallock::global_tas_lock(lock_flag);
	// Allocate the array
	data = argo::conew_array<int>(data_length);
	max = argo::conew_<int>(std::numeric_limits<int>::min());

	// Initialize the input data
	if (argo::node_id() == 0) {
		for (int i = 0; i < data_length; ++i)
			data[i] = i * 11 + 3;
	}
	
	// Make sure initialization is done and distribute the changes
	argo::barrier();

	// Start the threads
	int chunk = data_length / num_threads;
	std::vector<pthread_t> threads(local_num_threads);
	std::vector<thread_args> args(local_num_threads);
	for (int i = 0; i < local_num_threads; ++i) {
		int global_tid = (argo::node_id() * local_num_threads) + i;
		args[i].data_begin = global_tid * chunk;
		args[i].data_end = args[i].data_begin + chunk;
		pthread_create(&threads[i], nullptr, parmax, &args[i]);
	}
	// Join the threads
	for (auto &t : threads)
		pthread_join(t, nullptr);

	// Make sure everyone is done and get the changes
	argo::barrier();

	argo::codelete_array(data);
        delete lock;
	argo::codelete_(lock_flag);
	// Print the result
	if (argo::node_id() == 0)
		std::cout << "Max found to be " << *max << std::endl;
	assert(*max == ((data_length - 1) * 11 + 3));

	argo::finalize();
}

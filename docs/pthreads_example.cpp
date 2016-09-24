#include <cassert>
#include <limits>
#include <iostream>
#include <vector>

#include <pthread.h>

struct thread_args {
	int data_begin;
	int data_end;
};

int *data;
int max = std::numeric_limits<int>::min();

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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
	pthread_mutex_lock(&lock);
	if (local_max > max)
		max = local_max;
	pthread_mutex_unlock(&lock);

	return nullptr;
}

int main(int argc, char* argv[]) {
	int data_length = 160000;
	int num_threads = 16;

	// Allocate the array
	data = new int[data_length];

	// Initialize the input data
	for (int i = 0; i < data_length; ++i)
		data[i] = i * 11 + 3;

	// Start the threads
	int chunk = data_length / num_threads;
	std::vector<pthread_t> threads(num_threads);
	std::vector<thread_args> args(num_threads);
	for (int i = 0; i < num_threads; ++i) {
		args[i].data_begin = i * chunk;
		args[i].data_end = args[i].data_begin + chunk;
		pthread_create(&threads[i], nullptr, parmax, &args[i]);
	}
	// Join the threads
	for (auto& t : threads)
		pthread_join(t, nullptr);

	delete[] data;
	// Print the result
	std::cout << "Max found to be " << max << std::endl;
	assert(max == ((data_length - 1) * 11 + 3));
}

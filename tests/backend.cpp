/**
 * @file
 * @brief This file provides tests for the backends
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

// C headers
#include <mpi.h>
// C++ headers
#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>
// ArgoDSM headers
#include "argo.hpp"
#include "data_distribution/global_ptr.hpp"
// GoogleTest headers
#include "gtest/gtest.h"

/** @brief Global pointer to char */
using global_char = typename argo::data_distribution::global_ptr<char>;
/** @brief Global pointer to double */
using global_double = typename argo::data_distribution::global_ptr<double>;
/** @brief Global pointer to int */
using global_int = typename argo::data_distribution::global_ptr<int>;
/** @brief Global pointer to unsigned int */
using global_uint = typename argo::data_distribution::global_ptr<unsigned>;
/** @brief Global pointer to int pointer */
using global_intptr = typename argo::data_distribution::global_ptr<int*>;

/** @brief ArgoDSM memory size (16M) */
constexpr std::size_t size = 1<<24;
/** @brief ArgoDSM cache size */
constexpr std::size_t cache_size = size;
/** @brief ArgoDSM array size */
constexpr std::size_t array_size = 1<<19;

/** @brief Time to wait before assuming a deadlock has occured */
constexpr std::chrono::minutes deadlock_threshold{1};  // Choosen for no reason

/** @brief A random char constant */
constexpr char c_const = 'a';
/** @brief A random int constant */
constexpr int i_const = 42;
/** @brief A large random int constant */
constexpr unsigned j_const = 2124481224;
/** @brief A random double constant */
constexpr double d_const = 1.0/3.0 * 3.14159;

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class backendTest : public testing::Test, public ::testing::WithParamInterface<int> {
	protected:
		backendTest()  {
			argo::reset();
		}

		~backendTest() {
			argo::barrier();
		}
};


/**
 * @brief Test if atomic exchange writes the correct values
 *
 * The writes are performed by all the nodes at the same time
 */
TEST_F(backendTest, atomicXchgAll) {
	// @todo This test is disabled as OpenUCX only supports
	// atomic MPI operations of size 4 and 8 bytes.
	// global_char _c(argo::conew_<char>(0));
	// argo::backend::atomic::exchange(_c, c_const);
	// ASSERT_EQ(c_const, argo::backend::atomic::load(_c));

	global_int _i(argo::conew_<int>(0));
	argo::backend::atomic::exchange(_i, i_const);
	ASSERT_EQ(i_const, argo::backend::atomic::load(_i));

	global_uint _j(argo::conew_<unsigned>(0));
	argo::backend::atomic::exchange(_j, j_const);
	ASSERT_EQ(j_const, argo::backend::atomic::load(_j));

	global_double _d(argo::conew_<double>(0));
	argo::backend::atomic::exchange(_d, d_const);
	ASSERT_EQ(d_const, argo::backend::atomic::load(_d));
}

/**
 * @brief Test if atomic exchange writes the correct values
 *
 * The writes are only performed by one node
 */
TEST_F(backendTest, atomicXchgOne) {
	// @todo This test is disabled as OpenUCX only supports
	// atomic MPI operations of size 4 and 8 bytes.
	// global_char _c(argo::conew_<char>());
	// if (argo::backend::node_id() == 0)
	// 	argo::backend::atomic::exchange(_c, c_const);
	// argo::backend::barrier();
	// ASSERT_EQ(c_const, argo::backend::atomic::load(_c));

	global_int _i(argo::conew_<int>());
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::exchange(_i, i_const);
	argo::backend::barrier();
	ASSERT_EQ(i_const, argo::backend::atomic::load(_i));

	global_uint _j(argo::conew_<unsigned>());
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::exchange(_j, j_const);
	argo::backend::barrier();
	ASSERT_EQ(j_const, argo::backend::atomic::load(_j));
	argo::backend::barrier();
	// The following test is here to test implicit type conversions
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::exchange(_j, i_const);
	argo::backend::barrier();
	ASSERT_EQ(static_cast<unsigned>(i_const), argo::backend::atomic::load(_j));

	global_double _d(argo::conew_<double>());
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::exchange(_d, d_const);
	argo::backend::barrier();
	ASSERT_EQ(d_const, argo::backend::atomic::load(_d));
}

/**
 * @brief Test atomic stores
 */
TEST_F(backendTest, storeOne) {
	// @todo This test is disabled as OpenUCX only supports
	// atomic MPI operations of size 4 and 8 bytes.
	// global_char _c(argo::conew_<char>());
	// if (argo::backend::node_id() == 0)
	// 	argo::backend::atomic::store(_c, c_const);
	// argo::backend::barrier();
	// ASSERT_EQ(c_const, argo::backend::atomic::load(_c));

	global_int _i(argo::conew_<int>());
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::store(_i, i_const);
	argo::backend::barrier();
	ASSERT_EQ(i_const, argo::backend::atomic::load(_i));

	global_uint _j(argo::conew_<unsigned>());
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::store(_j, j_const);
	argo::backend::barrier();
	ASSERT_EQ(j_const, argo::backend::atomic::load(_j));
	argo::backend::barrier();
	// The following test is here to test implicit type conversions
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::store(_j, i_const);
	argo::backend::barrier();
	ASSERT_EQ(static_cast<unsigned>(i_const), argo::backend::atomic::load(_j));

	global_double _d(argo::conew_<double>());
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::store(_d, d_const);
	argo::backend::barrier();
	ASSERT_EQ(d_const, argo::backend::atomic::load(_d));
}

/**
 * @brief Test atomic loads
 *
 * These tests have node 0 write to a memory location and then all the nodes
 * reading from that memory location until they get the value that node 0 wrote.
 * To prevent deadlocks, there is a timeout in every spinloop.
 */
TEST_F(backendTest, loadOne) {
	std::chrono::system_clock::time_point max_time;
	// @todo This test is disabled as OpenUCX only supports
	// atomic MPI operations of size 4 and 8 bytes.
	// global_char _c(argo::conew_<char>());
	// if (argo::backend::node_id() == 0)
	// 	argo::backend::atomic::store(_c, c_const);
	// max_time = std::chrono::system_clock::now() + deadlock_threshold;
	// while(argo::backend::atomic::load(_c) != c_const)
	// 	ASSERT_LT(std::chrono::system_clock::now(), max_time);

	global_int _i(argo::conew_<int>());
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::store(_i, i_const);
	max_time = std::chrono::system_clock::now() + deadlock_threshold;
	while(argo::backend::atomic::load(_i) != i_const)
		ASSERT_LT(std::chrono::system_clock::now(), max_time);

	global_uint _j(argo::conew_<unsigned>());
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::store(_j, j_const);
	max_time = std::chrono::system_clock::now() + deadlock_threshold;
	while(argo::backend::atomic::load(_j) != j_const)
		ASSERT_LT(std::chrono::system_clock::now(), max_time);

	global_double _d(argo::conew_<double>());
	if (argo::backend::node_id() == 0)
		argo::backend::atomic::store(_d, d_const);
	max_time = std::chrono::system_clock::now() + deadlock_threshold;
	while(argo::backend::atomic::load(_d) != d_const)
		ASSERT_LT(std::chrono::system_clock::now(), max_time);
}

/**
 * @brief Test if atomic::exchange is indeed atomic
 *
 * A variable can be exchanged by all nodes but only one node should get the
 * initial value.
 */
TEST_F(backendTest, atomicXchgAtomicity) {
	global_int flag(argo::conew_<int>(0));
	int *rcs = argo::conew_array<int>(argo::number_of_nodes());

	// Initialize
	rcs[argo::node_id()] = 0;
	argo::barrier();

	// Do the exchange, try to get (flag == 0)
	int rc = argo::backend::atomic::exchange(flag, 1);
	rcs[argo::node_id()] = !rc;

	// Test if only one node succeeded
	argo::barrier();
	bool found = false;
	for (argo::num_nodes_t i = 0; i < argo::number_of_nodes(); ++i) {
		global_int _rcs(&rcs[i]);
		if (argo::backend::atomic::load(_rcs)) {
			ASSERT_FALSE(found);
			found = true;
		}
	}

	// Clean up
	argo::codelete_array(rcs);
}

/**
 * @brief Test the atomic::exchange visibility
 *
 * Go around in a circle signaling nodes using atomic::exchange and see if the
 * other shared data has also been made visible.
 */
TEST_F(backendTest, atomicXchgVisibility) {
	const int data_unset = 0xBEEF;
	const int data_set = 0x5555;
	const int flag_unset = 0xABBA;
	const int flag_set = 0x7777;

	int *shared_data = argo::conew_array<int>(argo::number_of_nodes());
	int *flag = argo::conew_array<int>(argo::number_of_nodes());
	int rc;

	int me = argo::node_id();
	int next = (me + 1) % argo::number_of_nodes();

	// Initialize
	shared_data[me] = data_unset;
	flag[me] = flag_unset;

	argo::barrier();

	// Send some data into the next node and set their flag as well
	shared_data[next] = data_set + next;
	global_int _nf(&flag[next]);
	rc = argo::backend::atomic::exchange(
		_nf, flag_set, argo::atomic::memory_order::release);
	// Nobody should have touched that but us
	ASSERT_EQ(flag_unset, rc);

	// Wait for data from the previous node
	int f;
	std::chrono::system_clock::time_point max_time =
		std::chrono::system_clock::now() + deadlock_threshold;
	global_int _f(&flag[me]);
	do {
		f = argo::backend::atomic::load(_f);
		// If we are over a minute in this loop, we have probably deadlocked.
		ASSERT_LT(std::chrono::system_clock::now(), max_time);
	} while (f == flag_unset);
	ASSERT_EQ(data_set + me, shared_data[me]);

	// Clean up
	argo::codelete_array(shared_data);
	argo::codelete_array(flag);
}

/**
 * @brief Test if exactly one CAS operation succeeds on the same data
 */
TEST_F(backendTest, atomicCASAtomicity) {
	global_uint flag(argo::conew_<unsigned>(0));
	unsigned *rcs = argo::conew_array<unsigned>(argo::number_of_nodes());

	// Initialize
	rcs[argo::node_id()] = 0;
	argo::barrier();

	// Do the exchange, try to get (flag == 0)
	bool success = argo::backend::atomic::compare_exchange(flag, 0, 1);
	rcs[argo::node_id()] = success;

	// Test if only one node succeeded
	argo::barrier();
	int count = 0;
	for (argo::num_nodes_t i = 0; i < argo::number_of_nodes(); ++i) {
		global_uint _rcs(&rcs[i]);
		if (argo::backend::atomic::load(_rcs)) {
			++count;
		}
	}
	ASSERT_EQ(1, count);

	// Clean up
	argo::codelete_array(rcs);
}

/**
 * @brief Test if exactly one CAS operation succeeds on the same data
 */
TEST_F(backendTest, atomicCASAtomicityStress) {
	global_int counter(argo::conew_<int>(0));

	// Do some fetch&adds using CAS
	for (int i = 0; i < 10000; ++i) {
		int c;
		do {
			c = argo::backend::atomic::load(counter);
		} while (!argo::backend::atomic::compare_exchange(counter, c, c+1));
	}

	argo::barrier();
	ASSERT_EQ(10000*argo::number_of_nodes(), argo::backend::atomic::load(counter));
}

/**
 * @brief Test the integer atomic fetch and add operation by doing lots of it
 */
TEST_F(backendTest, atomicFetchAddInt) {
	global_int counter(argo::conew_<int>(0));

	argo::barrier();
	for (int i = 0; i < 10000; ++i) {
		argo::backend::atomic::fetch_add(counter, 1);
	}

	argo::barrier();
	ASSERT_EQ(10000*argo::number_of_nodes(), argo::backend::atomic::load(counter));
}

/**
 * @brief Test the unsigned integer atomic fetch and add operation by doing lots of it
 */
TEST_F(backendTest, atomicFetchAddUInt) {
	// After all is done, we want the variable to have the maximum value an
	// unsigned integer can hold, since it should be bigger than the maximum
	// value a signed integer can.
	unsigned init_value =
		std::numeric_limits<unsigned>::max() - (10000 * argo::number_of_nodes());
	global_uint counter(argo::conew_<unsigned>(init_value));

	argo::barrier();
	for (int i = 0; i < 10000; ++i) {
		argo::backend::atomic::fetch_add(counter, 1);
	}

	argo::barrier();
	ASSERT_EQ(std::numeric_limits<unsigned>::max(), argo::backend::atomic::load(counter));
}

/**
 * @brief Test the floating atomic fetch and add operation by doing lots of it
 */
TEST_F(backendTest, atomicFetchAddFloat) {
	global_double counter(argo::conew_<double>(0));

	argo::barrier();
	for (int i = 0; i < 10000; ++i) {
		argo::backend::atomic::fetch_add(counter, 1);
	}

	argo::barrier();
	// If this fails, make sure it's not because of floating point precision
	// issues
	ASSERT_EQ(10000*argo::number_of_nodes(), argo::backend::atomic::load(counter));
}

/**
 * @brief Test if fetch&add handles pointers correctly
 */
TEST_F(backendTest, atomicFetchAddPointer) {
	global_intptr ptr(argo::conew_<int*>(nullptr));

	// This is just to see if we can handle pointers correctly, we don't care
	// about the atomicity etc
	if (argo::node_id() == 0) {
		int * old_ptr = argo::backend::atomic::fetch_add(ptr, 2);
		ASSERT_EQ(2, argo::backend::atomic::load(ptr) - old_ptr);
	}
}

/**
 * @brief Test selective coherence on a spinflag
 */
TEST_F(backendTest, selectiveSpin) {
	unsigned int* flag(argo::conew_<unsigned>(0));
	std::chrono::system_clock::time_point max_time =
		std::chrono::system_clock::now() + deadlock_threshold;
	// Set flag on node 0 and selectively downgrade it
	if (argo::node_id() == 0) {
		*flag = 1;
		argo::backend::selective_release(flag, sizeof(unsigned));
	}
	// Wait for the flag change to be visible on every node
	while(*flag != 1) {
		ASSERT_LT(std::chrono::system_clock::now(), max_time);
		argo::backend::selective_acquire(flag, sizeof(unsigned));
	}
}

/**
 * @brief Test selective coherence on multiple pages
 */
TEST_F(backendTest, selectiveArray) {
	unsigned int* flag(argo::conew_<unsigned>(0));
	// Allocate global array of 2MB size
	int* array = argo::conew_array<int>(array_size);
	std::chrono::system_clock::time_point max_time =
		std::chrono::system_clock::now() + deadlock_threshold;

	// Initialize
	if(argo::node_id() == 0) {
		for(std::size_t i = 0; i < array_size; i++) {
			array[i] = 0;
		}
	}
	argo::barrier();

	if(argo::node_id() == 0) {
		// Set each array element on node 0, then set flag
		for(std::size_t i = 0; i < array_size; i++) {
			array[i] = i_const;
		}
		argo::backend::selective_release(array, array_size*sizeof(int));
		*flag = 1;
		argo::backend::selective_release(flag, sizeof(unsigned));
	} else {
		// Read each element on remote nodes to ensure they are cached
		int tmp;
		const int max_total = i_const*array_size;
		for(std::size_t i = 0; i < array_size; i++) {
			tmp = array[i];
		}
		ASSERT_LE(tmp, max_total);
	}

	// Wait for the flag change to be visible on every node
	while(*flag != 1) {
		ASSERT_LT(std::chrono::system_clock::now(), max_time);
		argo::backend::selective_acquire(flag, sizeof(unsigned));
	}

	// Check the array values on every node
	argo::backend::selective_acquire(array, array_size*sizeof(int));
	int count = 0;
	const int expected = i_const*array_size;
	for(std::size_t i = 0; i < array_size; i++) {
		count += array[i];
	}
	ASSERT_EQ(count, expected);

	// Clean up
	argo::codelete_array(array);
}

/**
 * @brief Test selective coherence on unaligned acquires and releases
 */
TEST_F(backendTest, selectiveUnaligned) {
	const std::size_t ua_chunk_size = 256;
	// Allocate global array of 2MB size
	int* array = argo::conew_array<int>(array_size);
	unsigned int* flag(argo::conew_<unsigned>(0));
	std::chrono::system_clock::time_point max_time =
		std::chrono::system_clock::now() + deadlock_threshold;

	// Initialize
	if(argo::node_id() == 0) {
		for(std::size_t i = 0; i < array_size; i++) {
			array[i] = 0;
		}
	}
	argo::barrier();

	if(argo::node_id() == 0) {
		// Set array elements on node 0, then set flag
		// Write an unaligned chunk crossing a (remote node) boundary
		for(std::size_t i = ua_chunk_size*1807; i < ua_chunk_size*1809; i++) {
			array[i] = i_const;
		}
		argo::backend::selective_release(&array[ua_chunk_size*1807],
				(ua_chunk_size*2)*sizeof(int));

		*flag = 1;
		argo::backend::selective_release(flag, sizeof(unsigned));
	} else {
		// Read the set values on every other node to make sure that some
		// version of the pages involved are already cached
		int tmp = 0;
		const int max_total = i_const*ua_chunk_size*2;
		for(std::size_t i = ua_chunk_size*1807; i < ua_chunk_size*1809; i++) {
			tmp = array[i];
		}
		ASSERT_LE(tmp, max_total);
	}

	// Wait for the flag change to be visible on every node
	while(*flag != 1) {
		ASSERT_LT(std::chrono::system_clock::now(), max_time);
		argo::backend::selective_acquire(flag, sizeof(unsigned));
	}

	// Check the set array values on every node
	argo::backend::selective_acquire(&array[ua_chunk_size*1807],
			(ua_chunk_size*2)*sizeof(int));
	int count = 0;
	const int expected = i_const*ua_chunk_size*2;
	for(std::size_t i = 0; i < array_size; i++) {
		count += array[i];
	}
	ASSERT_EQ(count, expected);

	// Clean up
	argo::codelete_array(array);
}

/**
 * @brief Test coherence of writes on a randomly accessed global array with
 *        a node-wide barrier
 */
TEST_F(backendTest, randAccessesBarrierArray) {
	// Allocate global array of 2MB size
	int*const array = argo::conew_array<int>(array_size);

	// Allocate indices array and populate it
	std::vector<int> rand_index(array_size);
	std::iota(rand_index.begin(), rand_index.end(), 0);

	// Randomly shuffle the values of indices array
	constexpr unsigned seed{0};
	std::shuffle(rand_index.begin(), rand_index.end(), std::default_random_engine(seed));

	// Fetch node id and number of nodes
	const std::size_t rank = argo::node_id();
	const std::size_t nodes = argo::number_of_nodes();

	// Calculate the workload for each node
	const std::size_t chunk = array_size / nodes;
	const std::size_t beg = rank * chunk;
	const std::size_t end = (rank != nodes - 1) ? rank * chunk + chunk : array_size;

	// Compute node chunk
	for (std::size_t i = beg; i < end; ++i) {
		array[rand_index[i]] = rand_index[i];
	}
	// Barrier to synchronize and view latest values
	argo::barrier();

	// Calculate actual sum
	std::size_t sum{0};
	for (std::size_t i = 0; i < array_size; ++i) {
		sum += array[i];
	}
	// Calculate expected sum
	constexpr std::size_t expected = array_size * (array_size - 1) / 2;
	ASSERT_EQ(expected, sum);

	// Clean up
	argo::codelete_array(array);
}

/**
 * @brief Test coherence of writes on a randomly accessed global array with
 *        one bulky selective_release and an acquire
 */
TEST_F(backendTest, randAccessesBulkySelectiveReleaseAcquireArray) {
	// Allocate global array of 2MB size
	int*const array = argo::conew_array<int>(array_size);

	// Allocate indices array and populate it
	std::vector<int> rand_index(array_size);
	std::iota(rand_index.begin(), rand_index.end(), 0);

	// Randomly shuffle the values of indices array
	constexpr unsigned seed{0};
	std::shuffle(rand_index.begin(), rand_index.end(), std::default_random_engine(seed));

	// Fetch node id and number of nodes
	const std::size_t rank = argo::node_id();
	const std::size_t nodes = argo::number_of_nodes();

	// Calculate the workload for each node
	const std::size_t chunk = array_size / nodes;
	const std::size_t beg = rank * chunk;
	const std::size_t end = (rank != nodes - 1) ? rank * chunk + chunk : array_size;

	// Compute node chunk
	for (std::size_t i = beg; i < end; ++i) {
		array[rand_index[i]] = rand_index[i];
	}
	// Release and wait for all to release
	argo::backend::selective_release(array, array_size * sizeof(int));
	if (nodes > 1) {
		MPI_Barrier(MPI_COMM_WORLD);
	}

	// Acquire to view latest values
	argo::backend::acquire();

	// Calculate actual sum
	std::size_t sum{0};
	for (std::size_t i = 0; i < array_size; ++i) {
		sum += array[i];
	}
	// Calculate expected sum
	constexpr std::size_t expected = array_size * (array_size - 1) / 2;
	ASSERT_EQ(expected, sum);

	// Clean up
	argo::codelete_array(array);
}

/**
 * @brief Test coherence of writes on a randomly accessed global array with
 *        periodic selective_release and an acquire
 */
TEST_F(backendTest, randAccessesPeriodicSelectiveReleaseAcquireArray) {
	// Allocate global array of 2MB size
	int*const array = argo::conew_array<int>(array_size);

	// Allocate indices array and populate it
	std::vector<int> rand_index(array_size);
	std::iota(rand_index.begin(), rand_index.end(), 0);

	// Randomly shuffle the values of indices array
	constexpr unsigned seed{0};
	std::shuffle(rand_index.begin(), rand_index.end(), std::default_random_engine(seed));

	// Fetch node id and number of nodes
	const std::size_t rank = argo::node_id();
	const std::size_t nodes = argo::number_of_nodes();

	// Calculate the workload for each node
	const std::size_t chunk = array_size / nodes;
	const std::size_t beg = rank * chunk;
	const std::size_t end = (rank != nodes - 1) ? rank * chunk + chunk : array_size;

	// Compute node chunk
	for (std::size_t i = beg; i < end; ++i) {
		array[rand_index[i]] = rand_index[i];
		argo::backend::selective_release(&array[rand_index[i]], sizeof(int));
	}
	// Wait for all to compute and release
	if (nodes > 1) {
		MPI_Barrier(MPI_COMM_WORLD);
	}

	// Acquire to view latest values
	argo::backend::acquire();

	// Calculate actual sum
	std::size_t sum{0};
	for (std::size_t i = 0; i < array_size; ++i) {
		sum += array[i];
	}
	// Calculate expected sum
	constexpr std::size_t expected = array_size * (array_size - 1) / 2;
	ASSERT_EQ(expected, sum);

	// Clean up
	argo::codelete_array(array);
}

/**
 * @brief Test coherence of writes on a randomly accessed global page with
 *        a node-wide barrier
 */
TEST_F(backendTest, randAccessesBarrierPage) {
	// Allocate global page
	constexpr std::size_t array_size = 4096 / sizeof(unsigned char);
	unsigned char*const array = argo::conew_array<unsigned char>(array_size);

	// Allocate indices array and populate it
	std::vector<int> rand_index(array_size);
	std::iota(rand_index.begin(), rand_index.end(), 0);

	// Fetch node id and number of nodes
	const std::size_t rank = argo::node_id();
	const std::size_t nodes = argo::number_of_nodes();

	// Calculate the workload for each node
	const std::size_t chunk = array_size / nodes;
	const std::size_t beg = rank * chunk;
	const std::size_t end = (rank != nodes - 1) ? rank * chunk + chunk : array_size;

	// Set iterations and value per iteration
	constexpr int iters{4};
	constexpr int bytes[iters]{0x04, 0x08, 0x0C, 0xFF};

	// For `iters` iterations do...
	for (int it = 0; it < iters; ++it) {
		// Randomly shuffle the values of indices array
		std::shuffle(rand_index.begin(), rand_index.end(), std::default_random_engine(it));

		// Compute node chunk
		for (std::size_t i = beg; i < end; ++i) {
			array[rand_index[i]] = bytes[it];
		}
		// Barrier to synchronize and view latest values
		argo::barrier();

		// Calculate actual sum
		std::size_t sum{0};
		for (std::size_t i = 0; i < array_size; ++i) {
			sum += array[i];
		}
		// Calculate expected sum
		const std::size_t expected = array_size * bytes[it];
		ASSERT_EQ(sum, expected);

		// Wait for all to verify
		if (nodes > 1) {
			MPI_Barrier(MPI_COMM_WORLD);
		}
	}

	// Clean up
	argo::codelete_array(array);
}

/**
 * @brief Test coherence of writes on a randomly accessed global page with
 *        one bulky selective_release and an acquire
 */
TEST_F(backendTest, randAccessesBulkySelectiveReleaseAcquirePage) {
	// Allocate global page
	constexpr std::size_t array_size = 4096 / sizeof(unsigned char);
	unsigned char*const array = argo::conew_array<unsigned char>(array_size);

	// Allocate indices array and populate it
	std::vector<int> rand_index(array_size);
	std::iota(rand_index.begin(), rand_index.end(), 0);

	// Fetch node id and number of nodes
	const std::size_t rank = argo::node_id();
	const std::size_t nodes = argo::number_of_nodes();

	// Calculate the workload for each node
	const std::size_t chunk = array_size / nodes;
	const std::size_t beg = rank * chunk;
	const std::size_t end = (rank != nodes - 1) ? rank * chunk + chunk : array_size;

	// Set iterations and value per iteration
	constexpr int iters{4};
	constexpr int bytes[iters]{0x04, 0x08, 0x0C, 0xFF};

	// For `iters` iterations do...
	for (int it = 0; it < iters; ++it) {
		// Randomly shuffle the values of indices array
		std::shuffle(rand_index.begin(), rand_index.end(), std::default_random_engine(it));

		// Compute node chunk
		for (std::size_t i = beg; i < end; ++i) {
			array[rand_index[i]] = bytes[it];
		}
		// Release and wait for all to release
		argo::backend::selective_release(array, array_size * sizeof(unsigned char));
		if (nodes > 1) {
			MPI_Barrier(MPI_COMM_WORLD);
		}

		// Acquire to view latest values
		argo::backend::acquire();

		// Calculate actual sum
		std::size_t sum{0};
		for (std::size_t i = 0; i < array_size; ++i) {
			sum += array[i];
		}
		// Calculate expected sum
		const std::size_t expected = array_size * bytes[it];
		ASSERT_EQ(sum, expected);

		// Wait for all to verify
		if (nodes > 1) {
			MPI_Barrier(MPI_COMM_WORLD);
		}
	}

	// Clean up
	argo::codelete_array(array);
}

/**
 * @brief Test coherence of writes on a randomly accessed global page with
 *        periodic selective_release and an acquire
 */
TEST_F(backendTest, randAccessesPeriodicSelectiveReleaseAcquirePage) {
	// Allocate global page
	constexpr std::size_t array_size = 4096 / sizeof(unsigned char);
	unsigned char*const array = argo::conew_array<unsigned char>(array_size);

	// Allocate indices array and populate it
	std::vector<int> rand_index(array_size);
	std::iota(rand_index.begin(), rand_index.end(), 0);

	// Fetch node id and number of nodes
	const std::size_t rank = argo::node_id();
	const std::size_t nodes = argo::number_of_nodes();

	// Calculate the workload for each node
	const std::size_t chunk = array_size / nodes;
	const std::size_t beg = rank * chunk;
	const std::size_t end = (rank != nodes - 1) ? rank * chunk + chunk : array_size;

	// Set iterations and value per iteration
	constexpr int iters{4};
	constexpr int bytes[iters]{0x04, 0x08, 0x0C, 0xFF};

	// For `iters` iterations do...
	for (int it = 0; it < iters; ++it) {
		// Randomly shuffle the values of indices array
		std::shuffle(rand_index.begin(), rand_index.end(), std::default_random_engine(it));

		// Compute node chunk
		for (std::size_t i = beg; i < end; ++i) {
			array[rand_index[i]] = bytes[it];
			argo::backend::selective_release(&array[rand_index[i]], sizeof(unsigned char));
		}
		// Wait for all to compute and release
		if (nodes > 1) {
			MPI_Barrier(MPI_COMM_WORLD);
		}

		// Acquire to view latest values
		argo::backend::acquire();

		// Calculate actual sum
		std::size_t sum{0};
		for (std::size_t i = 0; i < array_size; ++i) {
			sum += array[i];
		}
		// Calculate expected sum
		const std::size_t expected = array_size * bytes[it];
		ASSERT_EQ(sum, expected);

		// Wait for all to verify
		if (nodes > 1) {
			MPI_Barrier(MPI_COMM_WORLD);
		}
	}

	// Clean up
	argo::codelete_array(array);
}

/**
 * @brief Test write buffer under load with random access patterns
 */
TEST_F(backendTest, writeBufferLoad) {
	const std::size_t num_ints = 4000000;  // 4M ints for just under total memory size
	const std::size_t num_writes = num_ints/20;  // Not too many random writes
	int* array = argo::conew_array<int>(num_ints);

	// Random device to ensure accesses are irregular in order to expose
	// random ordering between writebacks to different nodes
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_int_distribution<int> dist(0, num_ints-1);

	// Initialize write buffer
	if(argo::node_id() == 0) {
		for(std::size_t i = 0; i < num_ints; i++) {
			array[i] = 0;
		}
	}
	argo::barrier();

	// One node at a time, increment random elements num_writes times
	for(argo::num_nodes_t i = 0; i < argo::number_of_nodes(); i++) {
		if(i == argo::node_id()) {
			for(std::size_t j = 0; j < num_writes; j++) {
				array[dist(rng)] += 1;
			}
		}
		argo::barrier();
	}

	// Check that each node incremented num_writes elements
	if(argo::node_id() == 0) {
		int count = 0;
		int expected = num_writes*argo::number_of_nodes();
		for(std::size_t i = 0; i < num_ints; i++) {
			count += array[i];
		}
		ASSERT_EQ(count, expected);
	}
	// Clean up
	argo::codelete_array(array);
}


/**
 * @brief The main function that runs the tests
 * @param argc Number of command line arguments
 * @param argv Command line arguments
 * @return 0 if success
 */
int main(int argc, char **argv) {
	argo::init(size, cache_size);
	::testing::InitGoogleTest(&argc, argv);
	auto res = RUN_ALL_TESTS();
	argo::finalize();
	return res;
}

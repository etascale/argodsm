/**
 * @file
 * @brief This file provides tests using OpenMP in ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

// C headers
#include <limits.h>
#include <omp.h>
#include <unistd.h>
// C++ headers
#include <iostream>
// ArgoDSM headers
#include "allocators/collective_allocator.hpp"
#include "allocators/generic_allocator.hpp"
#include "allocators/null_lock.hpp"
#include "argo.hpp"
#include "backend/backend.hpp"
// GoogleTest headers
#include "gtest/gtest.h"

/** @brief Maximum number of OMP threads to run */
#define MAX_THREADS 16
/** @brief Numbers of iterations to run in each test */
#define ITER 10

/** @brief ArgoDSM memory size */
constexpr std::size_t size = 1<<30;
/** @brief ArgoDSM cache size */
constexpr std::size_t cache_size = size/8;

namespace mem = argo::mempools;
extern mem::global_memory_pool<>* default_global_mempool;

/** @brief Array size for testing */
int amount = 100000;

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class ompTest : public testing::Test {
	protected:
		ompTest()  {
			argo::reset();
		}

		~ompTest() {
			argo::barrier();
		}
};


/**
 * @brief Unittest that checks that data written 1 OpenMP thread per node by all threads after an ArgoDSM barrier
 */
TEST_F(ompTest, WriteAndRead) {
	int *arr = argo::conew_array<int>(amount);
	int node_id = argo_node_id();
	int nodecount = argo_number_of_nodes();

	ASSERT_GT(nodecount, 0);  // More than 0 nodes
	ASSERT_GE(node_id, 0);    // Node id non-negative

	int chunk = amount / nodecount;
	int start = chunk*node_id;
	int end = start+chunk;
	// Last node always iterates to the end of the array
	if(argo_node_id() == (argo_number_of_nodes()-1)) {
		end = amount;
	}
	argo::barrier();

	for(int n = 0; n < ITER; n++) {
		for(int i = 0; i < MAX_THREADS; i++) {
			omp_set_num_threads(i);

			#pragma omp parallel for
			for(int j = start; j < end; j++) {
				arr[j] = (i+42);
			}
			argo::barrier();

			#pragma omp parallel for
			for(int j = 0; j < amount; j++) {
				EXPECT_EQ(arr[j], (i+42));
			}
			argo::barrier();
		}
	}
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

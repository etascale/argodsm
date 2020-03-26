/**
 * @file
 * @brief This file provides tests using OpenMP in ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include <iostream>
#include <unistd.h>
#include <omp.h>
#include "argo.hpp"
#include "allocators/generic_allocator.hpp"
#include "allocators/collective_allocator.hpp"
#include "allocators/null_lock.hpp"
#include "backend/backend.hpp"
#include <limits.h>
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
			argo_reset();
			argo::barrier();

		}
		~ompTest() {
			argo::barrier();
		}
};


/**
 * @brief Unittest that checks that data written 1 OpenMP thread per node by all threads after an ArgoDSM barrier
 */
TEST_F(ompTest, WriteAndRead) {
	int i,j,n;
	int *arr = argo::conew_array<int>(amount);
	int node_id = argo_node_id(); // Node id
	int nodecount = argo_number_of_nodes(); // Number of nodes

	ASSERT_GT(nodecount,0); //More than 0 nodes
	ASSERT_GE(node_id,0); //Node id non-negative

	int chunk = amount / nodecount;
	int start = chunk*node_id;
	int end = start+chunk;
	if(argo_node_id() == (argo_number_of_nodes()-1)){ // Last node always iterates to the end of the array
		end = amount;
	}
	argo::barrier();

	for(n=0; n<ITER; n++){ //Run ITER times
		for(i=0; i<MAX_THREADS; i++){ //Up to MAX_THREADS, threads
			omp_set_num_threads(i);

#pragma omp parallel for
			for(j=start; j<end; j++){   
				arr[j]=(i+42); //Each entry written
			}
			argo::barrier();

#pragma omp parallel for
			for(j=0; j<amount; j++){
				EXPECT_EQ(arr[j],(i+42)); //Each thread checks for correctness
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

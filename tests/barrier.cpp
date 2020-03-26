/**
 * @file
 * @brief This file provides tests for the barrier synchronization
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "argo.hpp"
#include "gtest/gtest.h"

#include<vector>
#include<list>

/** @brief ArgoDSM memory size */
constexpr std::size_t size = 1<<30;
/** @brief ArgoDSM cache size */
constexpr std::size_t cache_size = size/8;

/** @brief Maximum number of threads to run in the stress tests */
constexpr int max_threads = 128;

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class barrierTest : public testing::Test, public ::testing::WithParamInterface<int> {
	protected:
		barrierTest()  {
			argo_reset();
			argo::barrier();

		}
		~barrierTest() {
			argo::barrier();
		}
};



/**
 * @brief Unittest that checks that the barrier call works
 */
TEST_F(barrierTest, simpleBarrier) {
	ASSERT_NO_THROW(argo::barrier());
}

/**
 * @brief Unittest that checks that the barrier call works with multiple threads
 */
TEST_P(barrierTest, threadBarrier) {
	std::vector<std::thread> thread_array;
	int node_local = 0;
	int* global = argo::conew_<int, argo::allocation::initialize>();
	for(int thread_count = 0; thread_count < GetParam(); thread_count++) {
		// add a thread
		thread_array.push_back(std::thread());
	}
	// run ALL threads from beginning
	int cnt = 1;
	for(auto& t : thread_array) {
		t = std::thread([=, &node_local]{
			for(int i = 0; i < GetParam(); i++) {
				ASSERT_NO_THROW(argo::barrier(GetParam()));
				if(cnt == i) { 
					node_local++;
					ASSERT_EQ(node_local, cnt);
				}
				if(cnt == i && ((i % argo::number_of_nodes()) == argo::node_id())) {
					(*global)++;
					ASSERT_EQ(*global, cnt);
				}
				ASSERT_NO_THROW(argo::barrier(GetParam()));
				ASSERT_EQ(node_local, i);
				ASSERT_EQ(*global, i);
			}
		});
		cnt++;
	}
	for(auto& t : thread_array) {
		t.join();
	}
}

/** @brief Test from 0 threads to max_threads, both inclusive */
INSTANTIATE_TEST_CASE_P(threadCount, barrierTest, ::testing::Range(0, max_threads+1));

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

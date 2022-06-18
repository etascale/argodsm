/**
 * @file
 * @brief This file provides tests for the barrier synchronization
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

// C++ headers
#include<list>
#include<vector>
// ArgoDSM headers
#include "argo.hpp"
// GoogleTest headers
#include "gtest/gtest.h"

/** @brief ArgoDSM memory size */
constexpr std::size_t size = 1<<30;
/** @brief ArgoDSM cache size */
constexpr std::size_t cache_size = size/8;
/** @brief Size of an ArgoDSM page */
constexpr std::size_t page_size = 4096;

/** @brief Maximum number of threads to run in the stress tests */
constexpr int max_threads = 128;

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class barrierTest : public testing::Test, public ::testing::WithParamInterface<int> {
	protected:
		barrierTest()  {
			argo::reset();
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
 * @brief Unittest that checks that pages that have been written
 * transition from S/SW to S following an upgrade barrier, and that
 * these pages remain in the node cache following self-invalidation.
 */
TEST_F(barrierTest, barrierUpgradeWriters) {
	std::size_t num_pages = 32;
	const char a = 'a';
	char* c_array = argo::conew_array<char>(page_size*num_pages);

	// Write data on node 0
	if(argo::node_id() == 0) {
		for(std::size_t i = 0; i < page_size*num_pages; i++) {
			c_array[i] = a;
		}
	}
	argo::barrier();

	// Read data on all nodes
	for(std::size_t i = 0; i < page_size*num_pages; i++) {
		ASSERT_EQ(c_array[i], a);
	}
	// Upgrade all non-private pages to shared
	argo::barrier_upgrade_writers();

	// Read data on all nodes and self-invalidate through a barrier
	for(std::size_t i = 0; i < page_size*num_pages; i++) {
		ASSERT_EQ(c_array[i], a);
	}
	argo::barrier();

	// Check that all nodes have all pages cached (or local)
	for(std::size_t n = 0; n < num_pages; n++) {
		ASSERT_TRUE(argo::backend::is_cached(&c_array[n*page_size]));
	}
}

/**
 * @brief Unittest that checks that pages that have been upgraded
 * before being accessed for the first time are correctly
 * recognized as state S, and not invalidated at self-invalidation.
 */
TEST_F(barrierTest, barrierReadUpgraded) {
	std::size_t num_pages = 32;
	const char a = 'a';
	char* c_array = argo::conew_array<char>(page_size*num_pages);

	// Write data on node 0
	if(argo::node_id() == 0) {
		for(std::size_t i = 0; i < page_size*num_pages; i++) {
			c_array[i] = a;
		}
	}
	// Upgrade all non-private pages to shared
	argo::barrier_upgrade_writers();

	// Read data on all nodes and self-invalidate through a barrier
	for(std::size_t i = 0; i < page_size*num_pages; i++) {
		ASSERT_EQ(c_array[i], a);
	}
	argo::barrier();

	// Check that all nodes have all pages cached (or local)
	for(std::size_t n = 0; n < num_pages; n++) {
		ASSERT_TRUE(argo::backend::is_cached(&c_array[n*page_size]));
	}
}

/**
 * @brief Unittest that checks that pages that have been upgraded
 * and subsequently written correctly transition from S to S/SW
 * and are invalidated from the node cache at self-invalidation.
 */
TEST_F(barrierTest, barrierDowngradeAfterUpgrade) {
	const std::size_t num_pages = 32;
	const char a = 'a';
	const char b = 'b';
	char* c_array = argo::conew_array<char>(page_size*num_pages);

	// Write data on node 0
	if(argo::node_id() == 0) {
		for(std::size_t i = 0; i < page_size*num_pages; i++) {
			c_array[i] = a;
		}
	}
	argo::barrier();

	// Read data on all nodes
	for(std::size_t i = 0; i < page_size*num_pages; i++) {
		ASSERT_EQ(c_array[i], a);
	}
	// Upgrade all non-private pages to shared
	argo::barrier_upgrade_writers();

	// Write to the same pages again
	if(argo::node_id() == 0) {
		for(std::size_t i = 0; i < page_size*num_pages; i++) {
			c_array[i] = b;
		}
	}
	argo::barrier();

	// Read data again all nodes
	for(std::size_t i = 0; i < page_size*num_pages; i++) {
		ASSERT_EQ(c_array[i], b);
	}
	argo::barrier();

	// Check that no node besides 0 has anything cached
	if(argo::node_id() != 0) {
		for(std::size_t n = 0; n < num_pages; n++) {
			if(argo::get_homenode(&c_array[n*page_size]) != argo::node_id()) {
				ASSERT_FALSE(argo::backend::is_cached(&c_array[n*page_size]));
			}
		}
	}
}

/**
 * @brief Unittest that checks that pages transition to state P following
 * an upgrade all barrier, and that all of these pages remain in the node
 * cache following self-invalidation.
 */
TEST_F(barrierTest, barrierUpgradeAll) {
	const std::size_t num_pages = 32;
	const char a = 'a';
	char* c_array = argo::conew_array<char>(page_size*num_pages);

	// Write data on node 0
	if(argo::node_id() == 0) {
		for(std::size_t i = 0; i < page_size*num_pages; i++) {
			c_array[i] = a;
		}
	}
	argo::barrier();

	// Read data on all nodes
	for(std::size_t i = 0; i < page_size*num_pages; i++) {
		ASSERT_EQ(c_array[i], a);
	}

	// Upgrade all pages to private
	argo::barrier_upgrade_all();

	// Check that no node has anything cached
	for(std::size_t n = 0; n < num_pages; n++) {
		if(argo::get_homenode(&c_array[n*page_size]) != argo::node_id()) {
			ASSERT_FALSE(argo::backend::is_cached(&c_array[n*page_size]));
		}
	}

	// Read data on all nodes
	for(std::size_t i = 0; i < page_size*num_pages; i++) {
		ASSERT_EQ(c_array[i], a);
	}
	argo::barrier();

	// Check that all nodes have all pages cached (or local)
	for(std::size_t n = 0; n < num_pages; n++) {
		ASSERT_TRUE(argo::backend::is_cached(&c_array[n*page_size]));
	}
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

/**
 * @file
 * @brief This file provides unit tests for the ArgoDSM prefetch mechanism
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

// C headers
#include <limits.h>
#include <unistd.h>
// C++ headers
#include <atomic>
#include <iostream>
// ArgoDSM headers
#include "allocators/collective_allocator.hpp"
#include "allocators/generic_allocator.hpp"
#include "allocators/null_lock.hpp"
#include "argo.hpp"
#include "backend/backend.hpp"
#include "env/env.hpp"
// GoogleTest headers
#include "gtest/gtest.h"

/** @brief ArgoDSM memory size */
constexpr std::size_t size = 1<<30;
/** @brief ArgoDSM cache size */
constexpr std::size_t cache_size = size/8;

namespace env = argo::env;
namespace dd = argo::data_distribution;
namespace mem = argo::mempools;
extern mem::global_memory_pool<>* default_global_mempool;

/** @brief A "random" char constant */
constexpr char c_const = 'a';

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class PrefetchTest : public testing::Test {
	protected:
		PrefetchTest() {
			argo::reset();
		}

		~PrefetchTest() {
			argo::barrier();
		}
};


/**
 * @brief Unit test that checks that there is no error when accessing
 * the first byte of the allocation.
 */
TEST_F(PrefetchTest, FirstPage) {
	std::size_t alloc_size = default_global_mempool->available();
	char *tmp = static_cast<char*>(collective_alloc(alloc_size));
	if(argo::node_id() == 0) {
		tmp[0] = c_const;
	}
	argo::barrier();
	ASSERT_EQ(c_const, tmp[0]);
}

/**
 * @brief Unit test that checks that there is no error when accessing
 * the last byte of the allocation.
 */
TEST_F(PrefetchTest, OutOfBounds) {
	std::size_t alloc_size = default_global_mempool->available();
	char *tmp = static_cast<char*>(collective_alloc(alloc_size));
	if(argo::node_id() == 0) {
		tmp[alloc_size-1] = c_const;
	}
	argo::barrier();
	ASSERT_EQ(c_const, tmp[alloc_size-1]);
}

/**
 * @brief Unit test that checks that there is no error when accessing
 * bytes on either side of a page boundary.
 */
TEST_F(PrefetchTest, PageBoundaries) {
	std::size_t alloc_size = default_global_mempool->available();
	char *tmp = static_cast<char*>(collective_alloc(alloc_size));
	std::size_t load_size = env::load_size();
	if(argo::node_id() == 0) {
		tmp[(PAGE_SIZE*load_size)-1] = c_const;
		tmp[PAGE_SIZE*load_size] = c_const;
	}
	argo::barrier();
	ASSERT_EQ(c_const, tmp[(PAGE_SIZE*load_size)-1]);
	ASSERT_EQ(c_const, tmp[PAGE_SIZE*load_size]);
}

/**
 * @brief Unit test that checks that pages are correctly prefetched.
 */
TEST_F(PrefetchTest, AccessPrefetched) {
	std::size_t alloc_size = default_global_mempool->available();
	char *tmp = static_cast<char*>(collective_alloc(alloc_size));
	std::size_t num_nodes = argo::number_of_nodes();
	std::size_t load_size = env::load_size();
	std::size_t block_size = env::allocation_block_size();

	std::size_t stride, start_page;
	/* Figure out boundaries for both block_size and load_size */
	if(dd::is_cyclic_policy()) {
		/*
		 * For blocked policies, at most one block is guaranteed
		 * to be contiguous in both global memory and backing store
		 */
		stride = std::min(block_size, load_size);
		start_page = 0;
	} else {
		/*
		 * For the first touch policiy, at most
		 * (alloc_size/PAGE_SIZE)/num_nodes are guaranteed to be contiguous 
		 * in both global memory and backing store.
		 */
		stride = (load_size < ((alloc_size/PAGE_SIZE)/num_nodes)) ?
				load_size : (alloc_size/PAGE_SIZE)/num_nodes - 1;
		start_page = 0;
	}
	std::size_t end_page = start_page+stride;

	/* We can only test prefetching if stride is larger than 1 (page) */
	if(stride > 1) {
		/* On node 0, initialize one fetchable block of pages */
		if(argo::node_id() == 0) {
			for(std::size_t page_num = start_page;
							page_num < end_page;
							page_num++) {
				for(std::size_t i = 0; i < PAGE_SIZE; i++) {
					tmp[page_num*PAGE_SIZE+i] = c_const;
				}
				/**
				 * This fence prevents both compiler and CPU reordering
				 * of stores (and loads) on a page level to ensure that
				 * tmp is contiguously mapped in the backing store. This
				 * is necessary to ensure a deterministic behaviour for
				 * the first-touch allocation policy.
				 */
				std::atomic_thread_fence(std::memory_order_seq_cst);
			}
		}
		/* Wait for init to finish */
		argo::barrier();

		/* On all nodes, check that after accessing the first page,
		 * all expected pages are either node local or cached. */
		ASSERT_EQ(tmp[start_page*PAGE_SIZE], c_const); // Access first page
		for(std::size_t page_num = start_page;
						page_num < end_page;
						page_num++) {
			ASSERT_TRUE(argo::backend::is_cached(&tmp[page_num*PAGE_SIZE]));
		}
		/* On all nodes, check that after accessing the first page,
		 * all data is correct. */
		for(std::size_t page_num = start_page;
						page_num < end_page;
						page_num++) {
			for(std::size_t i = 0; i < PAGE_SIZE; i++) {
				ASSERT_EQ(tmp[page_num*PAGE_SIZE+i], c_const);
			}
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

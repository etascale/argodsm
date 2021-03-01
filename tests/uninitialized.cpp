/**
 * @file
 * @brief This file provides unit tests for accessing ArgoDSM memory in various ways
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "argo.hpp"
#include "backend/backend.hpp"
#include "gtest/gtest.h"

/**
 * @brief Team process reads to the array worked in tests:
 *        - ReadUninitializedSinglenode
 * @param 1 To enable team process reads
 * @param 0 To disable team process reads
 */
#define TEAM_INIT 1

/** @brief ArgoDSM memory size */
constexpr std::size_t size = 1<<28;
/** @brief ArgoDSM cache size */
constexpr std::size_t cache_size = size/2;

namespace mem = argo::mempools;
extern mem::global_memory_pool<>* default_global_mempool;

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class UninitializedAccessTest : public testing::Test {

	protected:
		UninitializedAccessTest() {
			argo_reset();
			argo::barrier();
		}

		~UninitializedAccessTest() {
			argo::barrier();
		}
};

/**
 * @brief Function to distribute the workload among nodes
 * @param beg Points to the beginning of the chunk to be worked on
 * @param end Points to the end of the chunk to be worked on
 * @param loop_size Workload to distribute
 * @param beg_offset Offset at which the distribution loop begins
 * @param less_equal Set to `0` if the loop condition uses `<`
 *                   Set to `1` if the loop condition uses `<=`
 */
static inline void distribute
	(
		std::size_t& beg,
		std::size_t& end,
		const std::size_t& loop_size,
		const std::size_t& beg_offset,
    		const std::size_t& less_equal
	) {
	std::size_t chunk = loop_size / argo::number_of_nodes();
	beg = argo::node_id() * chunk + ((argo::node_id() == 0)
		? beg_offset 
		: less_equal);
	end = (argo::node_id() != argo::number_of_nodes() - 1)
		? argo::node_id() * chunk + chunk
		: loop_size;
}

/**
 * @brief Unittest that checks that there is no error when reading uninitialized coallocated memory.
 * @note this must be the first test, otherwise the memory is already "used"
 */
TEST_F(UninitializedAccessTest, ReadUninitializedSinglenode) {
	std::size_t allocsize = default_global_mempool->available();
	char *tmp = static_cast<char*>(collective_alloc(allocsize));

	/**
	 * @note Team process reads for the pages
	 *       to be distributed among nodes.
	 * @warning If only one process handles the
	 *          reads, under the first-touch me-
	 *          mory policy, it is possible that
	 *          this test fails since the backing
	 *          store of that node is exceeded.
	 */
#if TEAM_INIT == 1
	std::size_t beg, end;
	distribute(beg, end, allocsize, 0, 0);
	for(std::size_t i = beg; i < end; i++) {
		ASSERT_NO_THROW(asm volatile ("" : "=m" (tmp[i]) : "r" (tmp[i])));
	}
#else
	if(argo::node_id() == 0) {
		for(std::size_t i = 0; i < allocsize; i++) {
			ASSERT_NO_THROW(asm volatile ("" : "=m" (tmp[i]) : "r" (tmp[i])));
		}
	}
#endif
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

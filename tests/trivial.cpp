/**
 * @file
 * @brief This file provides unit tests for trivial sanity checks when using ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "argo.hpp"
#include "backend/backend.hpp"
#include "gtest/gtest.h"

/** @brief ArgoDSM memory size */
constexpr std::size_t size = 1<<20;
/** @brief ArgoDSM cache size */
constexpr std::size_t cache_size = size;

namespace mem = argo::mempools;

/**
 * @brief Unittest that checks nothing.
 * @note this test verifies that linking a trivial ArgoDSM program works
 */
TEST(trivialTest, alwaysPassl) {
	ASSERT_TRUE(true);
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

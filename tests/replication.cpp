/**
 * @file
 * @brief This file provides tests for the replicatio9n
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include <chrono>
#include <random>

#include "argo.hpp"
#include "data_distribution/global_ptr.hpp"

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
using global_intptr = typename argo::data_distribution::global_ptr<int *>;

/** @brief ArgoDSM memory size */
constexpr std::size_t size = 1 << 24; // 16MB
/** @brief ArgoDSM cache size */
constexpr std::size_t cache_size = size;

/** @brief Time to wait before assuming a deadlock has occured */
constexpr std::chrono::minutes deadlock_threshold{1}; // Choosen for no reason

/** @brief A random char constant */
constexpr char c_const = 'a';
/** @brief A random int constant */
constexpr int i_const = 42;
/** @brief A large random int constant */
constexpr unsigned j_const = 2124481224;
/** @brief A random double constant */
constexpr double d_const = 1.0 / 3.0 * 3.14159;

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class replicationTest : public testing::Test, public ::testing::WithParamInterface<int> {
protected:
	replicationTest() {
		argo_reset();
		argo::barrier();
	}
	~replicationTest() {
		argo::barrier();
	}
};

/**
 * @brief Test that replicated data can be fetched by its host node
 */
TEST_F(replicationTest, completeReplicationLocal) {
	// Test not relevant for single node
	if (argo_number_of_nodes() == 1) {
		ASSERT_TRUE(true);
		return;
	}

	global_char val;
	val = argo::conew_<char>('a');

	*val += 1;		 // All nodes does this!
	argo::barrier(); // Wait until writing is commited

	char receiver = 'z';
	if (argo::node_id() == argo_get_replnode(val.get())) {
		argo::backend::get_repl_data(val, (void *)(&receiver), 1);
		// printf("orignial val is %x = %c, repl val is %x = %c\n", *val, *val, receiver, receiver);
		ASSERT_EQ(*val, receiver);
	} else {
		// Not target node; automatically passing
		ASSERT_TRUE(true);
	}
}

/**
 * @brief Test that replicated data can be fetched by remote nodes
 */
TEST_F(replicationTest, completeReplicationRemote) {
	// Test not relevant for single node
	if (argo_number_of_nodes() == 1) {
		ASSERT_TRUE(true);
		return;
	}

	global_char val;
	val = argo::conew_<char>('g');

	*val += 1;
	argo::barrier();

	char receiver = 'z';
	if (argo::node_id() != argo_get_replnode(val.get())) {
		argo::backend::get_repl_data(val, (void *)&receiver, 1);
		// printf("orignial val is %x = %c, repl val is %x = %c\n", *val, *val, receiver, receiver);
		ASSERT_EQ(*val, receiver);
	} else {
		// Not target node; automatically passing
		ASSERT_TRUE(true);
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

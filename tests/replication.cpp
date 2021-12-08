/**
 * @file
 * @brief This file provides tests for the replicatio9n
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include <chrono>
#include <random>

#include "argo.hpp"
#include "env/env.hpp"
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

void _set_replication_policy(int id) {
	size_t repl_policy = argo::env::replication_policy();
	if (!repl_policy || repl_policy == !id) {
		setenv("ARGO_REPLICATION_POLICY", std::to_string(id).c_str(), 1);
		argo::env::init();
	}
}

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
 * @brief Simple test that a replicated char can be fetched by its host node using complete 
 * replication
 */
TEST_F(replicationTest, completeReplicationLocal) {
	if (argo_number_of_nodes() == 1) {
		return;
	}

	_set_replication_policy(0);

	char* val = argo::conew_<char>(c_const);

	if (argo::node_id() == 0) {
		*val += 1;
	}
	argo::barrier();

	char receiver = 'z';
	if (argo::node_id() == argo_get_replnode(val)) {
		argo::backend::get_repl_data(val, (void *)(&receiver), 1);
		ASSERT_EQ(*val, receiver);
	}
}

/**
 * @brief Test that a replicated char can be fetched by remote nodes using complete replication
 */
TEST_F(replicationTest, completeReplicationRemote) {
	if (argo_number_of_nodes() == 1) {
		return;
	}

	_set_replication_policy(0);

	char* val = argo::conew_<char>(c_const);

	if (argo::node_id() == 0) {
		*val += 1;
	}
	argo::barrier();

	char receiver = 'z';
	if (argo::node_id() != argo_get_replnode(val)) {
		argo::backend::get_repl_data(val, (void *)&receiver, 1);
		ASSERT_EQ(*val, receiver);
	}
}

/**
 * @brief Test that a replicated array can be fetched locally and remotely using complete 
 * replication
 */
TEST_F(replicationTest, completeReplicationArray) {
	if (argo_number_of_nodes() == 1) {
		return;
	}

	_set_replication_policy(0);

	const std::size_t array_size = 10;
	int* array = argo::conew_array<int>(array_size);
	int* receiver = new int[array_size];

	for (std::size_t i = 0; i < array_size; i++) {
		receiver[i] = 0;
	}

	if (argo::node_id() == 0) {
		for (std::size_t i = 0; i < array_size; i++) {
			array[i] = 1;
		}
	}
	argo::barrier();

	argo::backend::get_repl_data((char *) array, receiver, array_size * sizeof(*array));
	unsigned long count = 0;
	for (std::size_t i = 0; i < array_size; i++) {
		count += receiver[i];
	}
	ASSERT_EQ(count, array_size);

	delete [] receiver;
	argo::codelete_array(array);
}

/**
 * @brief Test that the replication policy can be changed at runtime
 */
TEST_F(replicationTest, replicationPolicyChange) {
	
	_set_replication_policy(1);
	ASSERT_EQ(argo::env::replication_policy(), (std::size_t)1);

}

/**
 * @brief Test that a single char can be rebuilt using erasure coding
 */
TEST_F(replicationTest, erasureCodingChar) {
	if (argo_number_of_nodes() == 1) {
		return;
	}

	_set_replication_policy(1);

	char* val = argo::conew_<char>(c_const);

	if (argo::node_id() == 0) {
		*val += 1;
	}
	argo::barrier();

	char receiver = 'z';
	if (argo::node_id() != argo_get_replnode(val)) {
		argo::backend::get_repl_data(val, (void *)&receiver, 1);
		ASSERT_EQ(*val, receiver);
	}
}

/**
 * @brief Test that a single char can be rebuilt using erasure coding
 */
TEST_F(replicationTest, rebuilding) {
	if (argo_number_of_nodes() == 1) {
		return;
	}

	/** Pseudo code: 
	 * define global char
	 * increment by 1 on node 0 (the home node for the char)
	 * kill node 0 directly after barrier
	 * on node 1, check that it is now the new home node for this char
	*/
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

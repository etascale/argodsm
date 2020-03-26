/**
 * @file
 * @brief This file provides unit tests for the ArgoDSM locks
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include <iostream>
#include <thread>
#include <unistd.h>

#include "argo.hpp"
#include "allocators/generic_allocator.hpp"
#include "allocators/collective_allocator.hpp"
#include "allocators/null_lock.hpp"
#include "backend/backend.hpp"
#include "synchronization/global_tas_lock.hpp"
#include "synchronization/cohort_lock.hpp"
#include "synchronization/intranode/mcs_lock.hpp"
#include "synchronization/intranode/ticket_lock.hpp"
#include "data_distribution/data_distribution.hpp"

#include <limits.h>
#include "gtest/gtest.h"

/** @brief ArgoDSM memory size */
constexpr std::size_t size = 1<<20;
/** @brief ArgoDSM cache size */
constexpr std::size_t cache_size = size;

/** @brief number of threads to spawn for some of the tests */
constexpr int nThreads = 16;
/** @brief number of itereations to run for some of the tests */
constexpr int iter = 10000;

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class LockTest : public testing::Test {

protected:
	/** @brief local ticket lock for testing */
	argo::locallock::ticket_lock *ticket_lock;
	/** @brief global MCS lock for testing */
	argo::locallock::mcs_lock *mcs_lock;
	/** @brief global cohort lock for testing */
	argo::globallock::cohort_lock *cohort_lock;
	/** @brief global tas lock for testing*/
	argo::globallock::global_tas_lock *global_tas_lock;
	/** @brief flag needed for the global tas lock*/
	bool* flag;
	/** @brief global counter used in several tests */
	int *counter;

	LockTest() {
		argo_reset();
		argo::barrier();
		flag = argo::conew_<bool>();
		global_tas_lock = new argo::globallock::global_tas_lock(flag);
		argo::barrier();
	}

	~LockTest() {
		argo::codelete_(flag);
		delete global_tas_lock;
		argo::barrier();
	}
};

/**
 * @brief Tests trylock functionality
 */
TEST_F(LockTest,TAS_trylock_all) {
	bool *did_increment;
	bool res;
	counter = argo::conew_<int>();
	did_increment = argo::conew_array<bool>(argo::number_of_nodes());

	/* Init a counter and increment array to 0 */
	if(argo::node_id() == 0){
		*counter = 0;
		for (int i = 0; i < argo::number_of_nodes(); i++){
			did_increment[i] = false;
		}
	}
	argo::barrier();

	/* All nodes try to take the lock and increment a shared counter */
	ASSERT_NO_THROW(res = global_tas_lock->try_lock());
	if(res){
		(*counter)++;
		did_increment[argo::node_id()] = true;
		ASSERT_NO_THROW(global_tas_lock->unlock());
	}
	argo::barrier();

	/* At least not more than number_of_nodes nodes can have incremented the counter */
	ASSERT_GE(argo::number_of_nodes(),*counter);

	int sum = 0;
	for (int i = 0; i < argo::number_of_nodes(); i++){
		if(did_increment[i])
			sum++;
	}

	/* Compares counter value to flags set by each node */
	ASSERT_EQ(sum,*counter);

	argo::codelete_(counter);
	argo::codelete_array(did_increment);
}


/** @brief Checks locking is working by implementing a custom barrier */
TEST_F(LockTest,TAS_lock_custom_barrier) {
	int tmp;

	/* Checks this many times if barrier was successful */
	long deadlock_threshold = 100000;

	counter = argo::conew_<int>();
	/* Init a counter to 0 */
	if(argo::node_id() == 0){
		*counter = 0;
	}
	argo::barrier();

	/* All nodes increment the counter */
	ASSERT_NO_THROW(global_tas_lock->lock());
	(*counter)++;
	ASSERT_NO_THROW(global_tas_lock->unlock());

	/* Poll for completion / all nodes incremented the counter */
	do{
		ASSERT_NO_THROW(global_tas_lock->lock());
		tmp = *counter;
		ASSERT_NO_THROW(global_tas_lock->unlock());
		/* Dont spin for too long */
		if((--deadlock_threshold) == 0){
			std::cout << "##### Risk for deadlock - exiting TAS_lock_custom_barrier test. #####" << std::endl;
			return;
		}
	}while(tmp != argo::number_of_nodes());

	ASSERT_EQ(*counter,argo::number_of_nodes());
	argo::codelete_(counter);
}

/**
 *@brief increments a shared counter to test locks
 *@param lock The lock to test
 *@param counter The counter to increment
 *@tparam LockType The type of the lock being tested
 *@return Unused variable
 */
template <typename LockType>
void increment_counter(LockType* lock, int* counter) {
	for (int i = 0; i < iter; i++) {
		lock->lock();
		(*counter)++;
		// if (*counter % 100 == 0)
			// std::cout << *counter << std::endl;
		lock->unlock();
	}
}

/**
 *@brief Increments a shared counter to test multiple locks
 *@param l1 The lock to test
 *@param l2 The lock to test
 *@param counter The counter to increment
 *@tparam LockType The type of the lock being tested
 *@return Unused variable
 */
template <typename LockType>
void increment_counter2(LockType* l1, LockType* l2, int* counter) {
	for (int i = 0; i < iter; i++) {
		l1->lock();
		l2->lock();
		(*counter)++;
		// if (*counter % 100 == 0)
			// std::cout << *counter << std::endl;
		l2->unlock();
		l1->unlock();
	}
}

/** @brief Checks if locking is working by incrementing a shared counter */
TEST_F(LockTest, StressMCSLock) {
	std::thread threads[nThreads];
	counter = new int(0);
	mcs_lock = new argo::locallock::mcs_lock();

	ASSERT_EQ(*counter, 0);

	for (int i = 0; i < nThreads; i++) {
		threads[i] = std::thread(
			increment_counter<argo::locallock::mcs_lock>, mcs_lock, counter);
	}
	for (int i = 0; i < nThreads; i++) {
		threads[i].join();
	}

	ASSERT_EQ(iter * nThreads, *counter);

	delete counter;
	delete mcs_lock;
}

/** @brief Checks if locking of multiple locks is working by incrementing a shared counter */
TEST_F(LockTest, StressMCSMultipleLocks) {
	std::thread threads[nThreads];
	int locks = 4;
	counter = new int(0);
	mcs_lock = new argo::locallock::mcs_lock[locks];
	argo::locallock::mcs_lock *global_lock = new argo::locallock::mcs_lock;

	ASSERT_EQ(*counter, 0);

	for (int i = 0; i < nThreads; i++) {
		threads[i] = std::thread(increment_counter2<argo::locallock::mcs_lock>,
			&mcs_lock[i % locks], global_lock, counter);
	}
	for (int i = 0; i < nThreads; i++) {
		threads[i].join();
	}

	ASSERT_EQ(iter * nThreads, *counter);

	delete counter;
	delete[] mcs_lock;
	delete global_lock;
}

/** @brief Checks locking is working by decrementing a shared counter */
TEST_F(LockTest,StressCohortLock) {
	std::thread threads[nThreads];
	counter = argo::conew_<int>(0);
	cohort_lock = new argo::globallock::cohort_lock();

	ASSERT_EQ(0, *counter);
	argo::barrier();
	for (int i = 0; i < nThreads; i++) {
		threads[i] = std::thread(
			increment_counter<argo::globallock::cohort_lock>, cohort_lock, counter);
	}
	for (int i = 0; i < nThreads; i++) {
		threads[i].join();
	}

	argo::barrier();
	ASSERT_EQ(iter * nThreads * argo::number_of_nodes(), *counter);
	argo::codelete_(counter);
	delete cohort_lock;
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

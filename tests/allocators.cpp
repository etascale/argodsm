/**
 * @file
 * @brief This file provides unit tests for the ArgoDSM allocators and memory pools
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include <iostream>
#include <tuple>

#include <limits.h>
#include <unistd.h>

#include "argo.hpp"
#include "allocators/generic_allocator.hpp"
#include "allocators/collective_allocator.hpp"
#include "allocators/null_lock.hpp"
#include "backend/backend.hpp"
#include "gtest/gtest.h"

/** @brief ArgoDSM memory size */
constexpr std::size_t size = 1<<30;
/** @brief ArgoDSM cache size */
constexpr std::size_t cache_size = size/8;

namespace mem = argo::mempools;
extern mem::global_memory_pool<>* default_global_mempool;

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class AllocatorTest : public testing::Test {

	protected:
		AllocatorTest() {
			argo_reset();
			argo::barrier();
		}

		~AllocatorTest() {
			argo::barrier();
		}
};




/**
 * @brief Unittest that checks that the global address space is at least as large as requested
 */
TEST_F(AllocatorTest, InitialSize) {
	ASSERT_GE(default_global_mempool->available(),size - mem::global_memory_pool<>::reserved);
}

/**
 * @brief Unittest that checks that allocating 0 bytes collectively is allowed
 */
TEST_F(AllocatorTest, CollectiveAllocZeroBytesCollectively) {
	ASSERT_NO_THROW(collective_alloc(0));
}

/**
 * @brief Unittest that checks that allocating 1 byte collectively is allowed
 */
TEST_F(AllocatorTest, CollectiveAllocOneByteCollectively) {
	ASSERT_NO_THROW(collective_alloc(1));
}

/**
 * @brief Unittest that checks that allocating a few different sizes collectively
 */
TEST_F(AllocatorTest, CollectiveCommonAlloc) {
	ASSERT_NO_THROW(collective_alloc(1));
	ASSERT_NO_THROW(collective_alloc(10));
	ASSERT_NO_THROW(collective_alloc(100));
	ASSERT_NO_THROW(collective_alloc(1000));
}

/**
 * @brief Unittest that checks that allocating 200MB*2 works
 */
TEST_F(AllocatorTest, Collective200MBTwiceAlloc) {
	ASSERT_NO_THROW(collective_alloc(209715200L));
	ASSERT_NO_THROW(collective_alloc(209715200L));
}

/**
 * @brief Unittest that checks that allocating the whole requested memory space collectively - also checks that the remaining memory is non-negative
 */
TEST_F(AllocatorTest, CollectiveAllocRequestedSize) {
	ASSERT_NO_THROW(collective_alloc(size - mem::global_memory_pool<>::reserved));
	ASSERT_GE(default_global_mempool->available(),std::size_t{0});
}

/**
 * @brief Unittest that checks that allocating all memory available collectively
 */
TEST_F(AllocatorTest, CollectiveAllocAll) {
	ASSERT_NO_THROW(collective_alloc(default_global_mempool->available()));
}




/**
 * @brief Unittest that checks that allocating all memory available collectively and then checks that allocating  more bytes will throw an exception
 */
TEST_F(AllocatorTest, CollectiveAllocAllAndExceedLimit) {
	ASSERT_NO_THROW(collective_alloc(default_global_mempool->available()));
	ASSERT_NO_THROW(collective_alloc(0));
	ASSERT_ANY_THROW(collective_alloc(1));
}


/**
 * @brief Unittest that checks that allocating more memory than what is available collectively
 */
TEST_F(AllocatorTest, CollectiveAllocExceedLimit) {
	std::size_t allocsize = 100000+default_global_mempool->available();
	ASSERT_ANY_THROW(collective_alloc(allocsize));
}

/**
 * @brief Unittest that checks that allocating more memory than what is available collectively
 */

TEST_F(AllocatorTest, CollectiveAllocLoopExceedLimit) {
	std::size_t allocsize=7;
	while(default_global_mempool->available() >= allocsize){
		ASSERT_NO_THROW(collective_alloc(allocsize));
		allocsize *= 2;
	}
	ASSERT_NO_THROW(collective_alloc(default_global_mempool->available()));
	ASSERT_ANY_THROW(collective_alloc(1));
}


/* Tests using dynamic allocator */


/**
 * @brief Unittest that checks that allocating 0 bytes dynamically by all nodes is allowed
 */
TEST_F(AllocatorTest, DynamicAllocZeroBytesDynamically) {
	ASSERT_NO_THROW(dynamic_alloc(0));
}

/**
 * @brief Unittest that checks that allocating 1 byte dynamically by all nodes is allowed
 */
TEST_F(AllocatorTest, DynamicAllocOneByteDynamically) {
	ASSERT_NO_THROW(dynamic_alloc(1));
}

/**
 * @brief Unittest that checks that allocating a few different sizes by all nodes dynamically
 */
TEST_F(AllocatorTest, DynamicCommonAlloc) {
	if(static_cast<unsigned int>(argo::number_of_nodes())*1111 >= default_global_mempool->available()){
		ASSERT_NO_THROW(dynamic_alloc(1));
		ASSERT_NO_THROW(dynamic_alloc(10));
		ASSERT_NO_THROW(dynamic_alloc(100));
		ASSERT_NO_THROW(dynamic_alloc(1000));
	}
}

/**
 * @brief Unittest that checks that node 0  allocating the whole requested memory space dynamically - also checks that the remaining memory is non-negative
 */
TEST_F(AllocatorTest, DynamicAllocRequestedSize) {
	if(argo::node_id() == 0){
		ASSERT_NO_THROW(dynamic_alloc(size - mem::global_memory_pool<>::reserved));
	}
	argo::barrier();
	ASSERT_GE(default_global_mempool->available(),std::size_t{0});
}

/**
 * @brief Unittest that checks that node N-1 allocating all memory available dynamically does not throw exceptions
 */
TEST_F(AllocatorTest, DynamicAllocAll) {
	if(argo::node_id() == argo::number_of_nodes()-1){
		ASSERT_NO_THROW(dynamic_alloc(default_global_mempool->available()));
	}
}


/**
 * @brief Unittest that checks that node N-1 allocating all memory available dynamically and then checks that allocating more bytes will throw an exception
 */
TEST_F(AllocatorTest, DynamicAllocAllAndExceedLimit) {
		if(argo::node_id() == argo::number_of_nodes()*0+1-1){
		ASSERT_NO_THROW(dynamic_alloc(default_global_mempool->available()));
		}
	//	ASSERT_NO_THROW(dynamic_alloc(0)); /* Should always be legal */
	argo::barrier();
		ASSERT_ANY_THROW(dynamic_alloc(1));
}


/**
 * @brief Unittest that checks that allocating more memory than what is available dynamically
 */
TEST_F(AllocatorTest, DynamicAllocExceedLimit) {
	std::size_t allocsize = 100000+default_global_mempool->available();
	ASSERT_ANY_THROW(dynamic_alloc(allocsize));
}

/**
 * @brief Unittest that checks that allocating more memory than what is available dynamically
 */

TEST_F(AllocatorTest, DynamicAllocLoopExceedLimit) {
	std::size_t allocsize=7;
	while(default_global_mempool->available() >= 2*allocsize && argo::node_id()==0){
		ASSERT_NO_THROW(dynamic_alloc(allocsize));
		allocsize *= 2;
	}
	argo::barrier();
	if(argo::node_id() == argo::number_of_nodes()-1) {
		ASSERT_NO_THROW(dynamic_alloc(default_global_mempool->available()));
	}
	argo::barrier();
	ASSERT_ANY_THROW(dynamic_alloc(1));
	argo::barrier();

}

/**
 * @brief Unittest that checks the case of node N-1 allocating all the
 * available memory dynamically
 */
TEST_F(AllocatorTest, DynamicAllocAllNodes) {
	std::size_t allocsize=7;

	/* Node N-1*/
	if(argo::node_id() == argo::number_of_nodes()-1) {
		while(default_global_mempool->available() > allocsize){
			ASSERT_NO_THROW(dynamic_alloc(allocsize));
			allocsize *= 2;
		}
		ASSERT_NO_THROW(dynamic_alloc(default_global_mempool->available()));
		ASSERT_ANY_THROW(dynamic_alloc(1));

	}
}

/* MIXED ALLOCATOR TESTS */

/**
 * @brief Array size of allocator tests
 */
const int entries = 10;

/**
 *@brief mixes dynamic and collective allocation and stores dynamic arrays in a collective arrays to communicate values, also stresses allocation.
 */
TEST_F(AllocatorTest, StoringDynamicArrayInCollective){
	int *dynamic_arr = argo::new_array<int>(entries);

	for(int i = 0; i < entries; i++){
		dynamic_arr[i] = argo::node_id()+i*10;
	}

	int **collective_arr = argo::conew_array<int*>(argo::number_of_nodes());

	collective_arr[argo::node_id()] = dynamic_arr;
	argo::barrier();

	for(int i = 0; i < argo::number_of_nodes(); i++){
		for(int j = 0; j < entries; j++){
			if(argo::node_id() == 0) {
				ASSERT_TRUE(collective_arr[i][j] == i+j*10);
			}
		}
	}
	argo::barrier();

	argo::codelete_array(collective_arr);
	argo::delete_array(dynamic_arr);

	argo::barrier();
	for(int i = 0; i < 100; i++){
		dynamic_arr = argo::new_array<int>(i*10);
		collective_arr = argo::conew_array<int *>(10);
		argo::delete_array(dynamic_arr);
		argo::codelete_array(collective_arr);
	}
	argo::barrier();

	dynamic_arr = argo::new_array<int>(entries);

	collective_arr = argo::conew_array<int*>(argo::number_of_nodes());

	int *collective_arr2 = argo::conew_array<int>(entries);

	argo::barrier();
	for(int i = 0; i < entries; i++){
		dynamic_arr[i] = argo::node_id()+i*11;
		if((i%argo::number_of_nodes()) == argo::node_id()){
				collective_arr2[i]=i;
		}
	}

	collective_arr[argo::node_id()] = dynamic_arr;
	argo::barrier();
	for(int i = 0; i < argo::number_of_nodes(); i++){
		for(int j = 0; j < entries; j++){
			//			std::cout << "collective_arr : " << j << "," << i << " => "<< collective_arr[i][j] << " i+j*11 " << i+(j*11)<< std::endl;
			//std::cout << "collective_arr2 : " << j << " => "<< collective_arr2[j] << std::endl;
			ASSERT_TRUE(collective_arr[i][j] == i+j*11);
			ASSERT_TRUE(collective_arr2[j] == j);
		}
	}
	argo::barrier();
}

/**
 * @brief Test if it is possible to delete a null pointer with no errors
 */
TEST_F(AllocatorTest, DeletingNull) {
	char * null_ptr = nullptr;
	ASSERT_NO_THROW(argo::delete_(null_ptr));
	ASSERT_NO_THROW(argo::delete_array(null_ptr));
	ASSERT_NO_THROW(argo::codelete_(null_ptr));
	ASSERT_NO_THROW(argo::codelete_array(null_ptr));

	null_ptr = NULL;
	ASSERT_NO_THROW(argo::delete_(null_ptr));
	ASSERT_NO_THROW(argo::delete_array(null_ptr));
	ASSERT_NO_THROW(argo::codelete_(null_ptr));
	ASSERT_NO_THROW(argo::codelete_array(null_ptr));

	ASSERT_NO_THROW(dynamic_free(NULL));
	ASSERT_NO_THROW(collective_free(NULL));
}

/**
 * @brief Test if the new_ function forwards the arguments properly
 */
TEST_F(AllocatorTest, PerfectForwardingDynamicNew) {
	int a = 1;
	int b = 2;
	int c = 3;

	using tt = std::tuple<int&, int&&, int>;

	tt *t = argo::new_<tt>(a, std::move(b), c);

	std::get<0>(*t) = 2;
	std::get<1>(*t) = 3;
	std::get<2>(*t) = 4;

	ASSERT_EQ(2, a);
	ASSERT_EQ(3, b);
	ASSERT_EQ(3, c);
}

/**
 * @brief Test if the conew_ function forwards the arguments properly
 */
TEST_F(AllocatorTest, PerfectForwardingCollectiveNew) {
	int a = 1;
	int b = 2;
	int c = 3;

	using tt = std::tuple<int&, int&&, int>;
	using global_tt = typename argo::data_distribution::global_ptr<tt>;

	tt *t = argo::conew_<tt>(a, std::move(b), c);
	global_tt global_t(t);

	// XXX This only works if the home node of `t` is the one calling the constructor
	if (argo::node_id() == global_t.node()) {
		std::get<0>(*global_t) = 2;
		std::get<1>(*global_t) = 3;
		std::get<2>(*global_t) = 4;

		ASSERT_EQ(2, a);
		ASSERT_EQ(3, b);
		ASSERT_EQ(3, c);
	}
}

/**
 * @brief Test the "parser" for the allocation parameters
 */
TEST_F(AllocatorTest, AllocationParametersParsing) {
	using namespace argo;
	using namespace argo::_internal;

	ASSERT_TRUE((alloc_param_in<allocation::initialize,
		allocation::initialize>::value));
	ASSERT_TRUE((alloc_param_in<allocation::no_initialize,
		allocation::no_initialize>::value));
	ASSERT_TRUE((alloc_param_in<allocation::deinitialize,
		allocation::deinitialize>::value));
	ASSERT_TRUE((alloc_param_in<allocation::no_deinitialize,
		allocation::no_deinitialize>::value));
	ASSERT_TRUE((alloc_param_in<allocation::synchronize,
		allocation::synchronize>::value));
	ASSERT_TRUE((alloc_param_in<allocation::no_synchronize,
		allocation::no_synchronize>::value));

	using all_yes = alloc_params<allocation::initialize,
		allocation::deinitialize, allocation::synchronize>;
	ASSERT_TRUE(all_yes::initialize);
	ASSERT_TRUE(all_yes::deinitialize);
	ASSERT_TRUE(all_yes::synchronize);
	ASSERT_FALSE(all_yes::no_initialize);
	ASSERT_FALSE(all_yes::no_deinitialize);
	ASSERT_FALSE(all_yes::no_synchronize);

	using all_no = alloc_params<allocation::no_initialize,
		allocation::no_deinitialize, allocation::no_synchronize>;
	ASSERT_FALSE(all_no::initialize);
	ASSERT_FALSE(all_no::deinitialize);
	ASSERT_FALSE(all_no::synchronize);
	ASSERT_TRUE(all_no::no_initialize);
	ASSERT_TRUE(all_no::no_deinitialize);
	ASSERT_TRUE(all_no::no_synchronize);

	using just_one = alloc_params<allocation::synchronize>;
	ASSERT_FALSE(just_one::initialize);
	ASSERT_FALSE(just_one::deinitialize);
	ASSERT_TRUE(just_one::synchronize);
	ASSERT_FALSE(just_one::no_initialize);
	ASSERT_FALSE(just_one::no_deinitialize);
	ASSERT_FALSE(just_one::no_synchronize);

	using none = alloc_params<>;
	ASSERT_FALSE(none::initialize);
	ASSERT_FALSE(none::deinitialize);
	ASSERT_FALSE(none::synchronize);
	ASSERT_FALSE(none::no_initialize);
	ASSERT_FALSE(none::no_deinitialize);
	ASSERT_FALSE(none::no_synchronize);
}

/**
 * @brief Test if initialization works properly
 *
 * This test might succeed even if initialization doesn't work, but if it is run
 * enough times it should catch the errors.
 */
TEST_F(AllocatorTest, NewInitialization) {
	int arr_size = 10;

	int *a;
	// Explicit initialization allocation parameter
	a = argo::new_<int, argo::allocation::initialize>();
	ASSERT_EQ(0, *a);
	argo::delete_(a);
	// Expliticit initialization parameter
	a = argo::new_<int>(42);
	ASSERT_EQ(42, *a);

	// Initialization in collective allocation assumes synchronization as well
	int *ac;
	ac = argo::conew_<int, argo::allocation::initialize>();
	ASSERT_EQ(0, *ac);
	argo::codelete_(a);
	ac = argo::conew_<int>(21);
	ASSERT_EQ(21, *ac);

	int *as;
	as = argo::new_array<int, argo::allocation::initialize>(arr_size);
	for (int i = 0; i < arr_size; ++i)
		ASSERT_EQ(0, as[i]);

	// Initialization in collective allocation assumes synchronization as well
	int *acs;
	acs = argo::conew_array<int, argo::allocation::initialize>(arr_size);
	for (int i = 0; i < arr_size; ++i)
		ASSERT_EQ(0, acs[i]);
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

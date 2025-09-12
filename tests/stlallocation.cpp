/**
 * @file
 * @brief This file provides tests using C++ interfaces in ArgoDSM
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

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class cppTest : public testing::Test {
	protected:
		cppTest()  {
			argo::reset();
		}

		~cppTest() {
			argo::barrier();
		}
};


/**
 * @brief Unittest that checks that an STL list can be allocated globally and populated
 */
TEST_F(cppTest, simpleList) {
	using my_list = std::list<int, argo::allocators::dynamic_allocator<int>>;

	my_list* l = argo::conew_<my_list>();

	for(unsigned int i = 0; i < argo_number_of_nodes(); i++) {
		if(argo_node_id() == i) {
			ASSERT_NO_THROW(l->push_back(i));
		}
		argo::barrier();
	}

	int id = 0;
	for (auto elem : *l) {
		ASSERT_EQ(elem, id);
		id++;
	}
}

/**
 * @brief Unittest that checks that an STL vector can be allocated globally and populated
 */
TEST_F(cppTest, simpleVector) {
	using my_vector = std::vector<int, argo::allocators::dynamic_allocator<int>>;

	my_vector* v = argo::conew_<my_vector>();

	for(unsigned int i = 0; i < argo_number_of_nodes(); i++) {
		if(argo_node_id() == i) {
			ASSERT_NO_THROW(v->push_back(i));
		}
		argo::barrier();
	}

	int id = 0;
	for (auto elem : *v) {
		ASSERT_EQ(elem, id);
		id++;
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

/**
 * @file
 * @brief This file provides tests using C++ interfaces in ArgoDSM
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

/**
 * @brief Class for the gtests fixture tests. Will reset the allocators to a clean state for every test
 */
class cppTest : public testing::Test {
	protected:
		cppTest()  {
			argo_reset();
			argo::barrier();

		}
		~cppTest() {
			argo::barrier();
		}
};



/**
 * @brief Unittest that checks that an STL list can be allocated globally and populated
 */
TEST_F(cppTest, simpleList) {
	using namespace std;
	using namespace argo;
	using namespace argo::allocators;
	using my_list = std::list<int, dynamic_allocator<int>>;

	my_list* l = conew_<my_list>();

	for(int i=0; i<argo_number_of_nodes(); i++){
		if(argo_node_id() == i) {
			ASSERT_NO_THROW(l->push_back(i));
		}
		barrier();
	}

	int id = 0;
	for (auto elem : *l){
		ASSERT_EQ(elem,id);
		id++;
	}
}



/**
 * @brief Unittest that checks that an STL vector can be allocated globally and populated
 */
TEST_F(cppTest, simpleVector) {
	using namespace std;
	using namespace argo;
	using namespace argo::allocators;
	using my_vector = std::vector<int, dynamic_allocator<int>>;

	my_vector* v = conew_<my_vector>();

	for(int i=0; i<argo_number_of_nodes(); i++){
		if(argo_node_id() == i) {
			ASSERT_NO_THROW(v->push_back(i));
		}
		barrier();
	}

	int id = 0;
	for (auto elem : *v){
		ASSERT_EQ(elem,id);
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

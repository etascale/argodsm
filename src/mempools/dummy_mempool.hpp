/**
 * @file
 * @brief This file provides a dummy memory pool for testing purposes
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 * @note not intended for production use
 */

#ifndef SRC_MEMPOOLS_DUMMY_MEMPOOL_HPP_
#define SRC_MEMPOOLS_DUMMY_MEMPOOL_HPP_

namespace argo {
	/**
	 * @brief Dummy memory pool
	 * @todo replace with code to use mmap for ArgoDSM global address space
	 */
	class memory_pool {
		private:
			/** @todo Documentation */
			char* memory;
			/** @todo Documentation */
			std::size_t max_size;
			/** @todo Documentation */
			std::size_t offset;
		public:
			/** type of allocation failures within this memory pool */
			using bad_alloc = std::bad_alloc;

			/**
			 * @brief Default constructor: initializes memory on heap and sets offset to 0
			 * @param size The amount of memory in the pool
			 */
			explicit memory_pool(std::size_t size) : memory(new char[size]), max_size(size), offset{0} {}

			/**
			 * @brief Reserve more memory
			 * @param size Amount of memory reserved
			 * @return The pointer to the first byte of the newly reserved memory area
			 * @todo move size check to separate function?
			 */
			char* reserve(std::size_t size) {
				if(offset+size > max_size) throw bad_alloc();
				char* ptr = &memory[offset];
				offset += size;
				return ptr;
			}

			/**
			 * @brief fail to grow the memory pool
			 * @param size minimum size to grow
			 */
			void grow(std::size_t size) {
				throw std::bad_alloc();
			}

			/**
			 * @brief check remaining available memory in pool
			 * @return remaining bytes in memory pool
			 */
			std::size_t available() {
				return max_size - offset;
			}
	};
} // namespace argo

#endif // SRC_MEMPOOLS_DUMMY_MEMPOOL_HPP_

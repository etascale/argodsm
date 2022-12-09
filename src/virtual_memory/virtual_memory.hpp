/**
 * @file
 * @brief This file provides facilities for handling virtual memory and virtual address space
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_VIRTUAL_MEMORY_VIRTUAL_MEMORY_HPP_
#define ARGODSM_SRC_VIRTUAL_MEMORY_VIRTUAL_MEMORY_HPP_

#include <utility>

namespace argo {
namespace virtual_memory {

/**
 * @brief initialize the ArgoDSM virtual address space
 * @todo virtual address space handling should be wrapped into an object
 */
void init();

/**
 * @brief get a pointer to the ArgoDSM virtual memory
 * @return the pointer to ArgoDSM virtual memory
 */
void* start_address();

/**
 * @brief Get the usable size of the ArgoDSM virtual memory
 * @note Up to half of the virtual memory is earmarked for internal structures
 * @return the usable size of the ArgoDSM virtual memory
 */
std::size_t size();

/**
 * @brief allocate memory that can be mapped into ArgoDSM virtual address space later
 * @param alignment the alignment of the allocation
 * @param size size of the allocation
 * @return a pointer to the new memory allocation
 * @details this will allocate memory that is guaranteed to work with map_memory().
 *          Any memory allocated through other means may not be possible to map
 *          into the visible ArgoDSM virtual memory space later.
 */
void* allocate_mappable(std::size_t alignment, std::size_t size);

/**
 * @brief map memory into ArgoDSM virtual address space
 * @param addr the address to map to
 * @param size the size of the mapping
 * @param offset the offset into the backing memory
 * @param prot protection flags for the mapping
 */
void map_memory(void* addr, std::size_t size, std::size_t offset, int prot);

}  // namespace virtual_memory
}  // namespace argo

#endif  // ARGODSM_SRC_VIRTUAL_MEMORY_VIRTUAL_MEMORY_HPP_

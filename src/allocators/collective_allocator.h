/**
 * @file
 * @brief This file provides the collective allocator for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_ALLOCATORS_COLLECTIVE_ALLOCATOR_H_
#define ARGODSM_SRC_ALLOCATORS_COLLECTIVE_ALLOCATOR_H_

/**
 * @brief basic collective allocation function, for C interface
 * @param size number of bytes to allocate
 * @return pointer to the newly allocated memory
 */
void* collective_alloc(size_t size);

/**
 * @brief basic free function for collective allocations, for C interface
 * @param ptr pointer to free
 */
void collective_free(void* ptr);

#endif  // ARGODSM_SRC_ALLOCATORS_COLLECTIVE_ALLOCATOR_H_

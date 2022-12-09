/**
 * @file
 * @brief This file contains the ArgoDSM virtual memory limits
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_VIRTUAL_MEMORY_VM_LIMITS_HPP_
#define ARGODSM_SRC_VIRTUAL_MEMORY_VM_LIMITS_HPP_

/**
 * @brief The start of the ArgoDSM virtual memory space
 * @note This value assumes x86_64 architecture
 *
 * The ArgoDSM virtual memory space leaves the first 1/6 for local use.
 */
char* const ARGO_VM_START = reinterpret_cast<char*>(0x155555554000l);

/**
 * @brief The maximum size of the ArgoDSM virtual memory space
 * @note This value assumes x86_64 architecture
 *
 * ArgoDSM reserves up to half of the available user-space virtual memory. A
 * particular VM implementation may choose to reserve less than this number.
 * In combination with @ref{ARGO_VM_START}, this ensures that the final third
 * of the virtual memory is left for PIE loads, heap, shared libraries and
 * the stack among other things.
 */
constexpr ptrdiff_t ARGO_VM_SIZE = 0x400000000000l;

#endif  // ARGODSM_SRC_VIRTUAL_MEMORY_VM_LIMITS_HPP_

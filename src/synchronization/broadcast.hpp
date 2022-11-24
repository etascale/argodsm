/**
 * @file
 * @brief This file provides a broadcasting facility for a single value
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_SYNCHRONIZATION_BROADCAST_HPP_
#define ARGODSM_SRC_SYNCHRONIZATION_BROADCAST_HPP_

#include "backend/backend.hpp"
#include "types/types.hpp"

namespace argo {
namespace synchronization {

/**
 * @brief broadcast a single value
 * @tparam T the type of the value to broadcast
 * @param from the source node of the broadcast
 * @param value to broadcast
 * @return The value broadcasted
 * @todo is this a collective call?
 * @todo should this be merged with the backend interface?
 */
template<typename T>
T broadcast(node_id_t from, const T value) {
	T v = value;
	backend::broadcast(from, &v);
	return v;
}

#if 0
/**
 * @brief synchronize a global memory address across all ArgoDSM nodes
 * @tparam from the source node of the broadcast
 * @tparam T the type of the value to broadcast
 * @param value
 */
template<node_id_t from, typename T>
void synchronize(T* ptr) {
	backend::broadcast(from, ptr);
}
template<typename T>
void synchronize(T* ptr) {
	auto from = backend::node_id();
	backend::broadcast(from, ptr);
}
#endif

}  // namespace synchronization
}  // namespace argo

#endif  // ARGODSM_SRC_SYNCHRONIZATION_BROADCAST_HPP_

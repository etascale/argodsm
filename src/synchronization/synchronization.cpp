/**
 * @file
 * @brief This file implements synchronization primitives for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "../backend/backend.hpp"

namespace argo {
	void barrier(std::size_t threadcount) {
		backend::barrier(threadcount, 0);
	}

	void barrier_upgrade_writers(std::size_t threadcount) {
		backend::barrier(threadcount, 1);
	}

	void barrier_upgrade_all(std::size_t threadcount) {
		backend::barrier(threadcount, 2);
	}
} // namespace argo

extern "C" {
	void argo_barrier(size_t threadcount) {
		argo::barrier(threadcount);
	}

	void argo_barrier_upgrade_writers(size_t threadcount) {
		argo::barrier_upgrade_writers(threadcount);
	}

	void argo_barrier_upgrade_all(size_t threadcount) {
		argo::barrier_upgrade_all(threadcount);
	}
}

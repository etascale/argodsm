/**
 * @file
 * @brief MCS lock for local use
 * @todo Better documentation
 * @todo namespace use and indentation in this file is inconsistent with other files
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */
#ifndef ARGODSM_SRC_SYNCHRONIZATION_INTRANODE_MCS_LOCK_HPP_
#define ARGODSM_SRC_SYNCHRONIZATION_INTRANODE_MCS_LOCK_HPP_

#include <atomic>
#include <map>

namespace argo {
namespace locallock {

/**
 * @brief MCS mutual exclusion lock (intranode)
 *
 * This lock implementation is based on a C implementation by Kjell Winbland
 * https://github.com/kjellwinblad/qd_lock_lib
 * This lock only works for individual nodes and not across multiple nodes.
 *
 * @warning The MCS need to be locked and unlocked by the same thread.
 */
class mcs_lock {
private:
	/** @brief Thread nodes for the lock */
	struct mcs_node {
		/** @brief Construct a new node */
		mcs_node() : next(nullptr), locked(false) {}

		/** @brief Node to wakeup at lock release */
		std::atomic<mcs_node*> next;
		/** @brief true if the lock is busy */
		std::atomic<bool> locked;
	};

	/** @brief Last node (thread) to try and acquire the lock */
	std::atomic<mcs_node*> _tail;

	/** @brief Local nodes for the different locks each thread can have */
	thread_local static std::map<mcs_lock*, mcs_node> selfs;

public:
	/**
	 * @brief Construct an MCS lock
	 */
	mcs_lock() : _tail(nullptr) {}

	/**
	 * @brief Acquire the MCS lock.
	 */
	void lock();

	/**
	 * @brief Try to acquire the lock, but do not block
	 * @return true if the lock was acquired successfully, false
	 * otherwise
	 */
	bool try_lock();

	/**
	 * @brief Release the MCS lock
	 */
	void unlock();

	/**
	 * @brief Check if the lock is contented
	 * @return true if the lock is contented
	 */
	bool is_contended();
};

} // namespace locallock
} // namespace argo

#endif // ARGODSM_SRC_SYNCHRONIZATION_INTRANODE_MCS_LOCK_HPP_

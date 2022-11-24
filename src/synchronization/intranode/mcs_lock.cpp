/**
 * @file
 * @brief This file provides an MCS-style lock for the ArgoDSM system
 * @todo Better documentation
 * @todo namespace use and indentation in this file is inconsistent with other files
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include <thread>

#include "mcs_lock.hpp"

namespace argo {
namespace locallock {

void mcs_lock::lock() {
	mcs_node* self = &selfs[this];

	// See if the lock is locked
	self->next.store(nullptr, std::memory_order_relaxed);
	mcs_node* predecessor = _tail.exchange(self, std::memory_order_release);
	if (predecessor != nullptr) {
		// The lock was locked. Put ourselves in the queue...
		self->locked.store(true, std::memory_order_relaxed);
		predecessor->next.store(self, std::memory_order_release);
		// ... and wait
		while (self->locked.load(std::memory_order_acquire))
			std::this_thread::yield();
	}
}

/**
 * @brief Try to acquire the lock, but do not block
 * @return true if the lock was acquired successfully, false
 * otherwise
 */
bool mcs_lock::try_lock() {
	if (_tail.load(std::memory_order_acquire) != nullptr) {
		return false;
	} else {
		mcs_node* self = &selfs[this];
		self->next.store(nullptr, std::memory_order_relaxed);
		return _tail.compare_exchange_strong(self, nullptr);
	}
}

/**
 * @brief Release the MCS lock
 */
void mcs_lock::unlock() {
	mcs_node* self = &selfs[this];
	mcs_node* also_self = self;  // atomic CAS changes this

	// See if there is anyone waiting
	if (self->next.load(std::memory_order_acquire) == nullptr) {
		// Nobody is waiting, unlock the lock and we are done
		if (_tail.compare_exchange_strong(also_self, nullptr)) {
			return;
		}
		// Someone is actually waiting, but we don't know who
		while (self->next.load(std::memory_order_acquire) == nullptr)
			std::this_thread::yield();
	}

	// Notify whomever is next
	self->next.load()->locked.store(false, std::memory_order_release);
}

/**
 * @brief Check if the lock is contented
 */
bool mcs_lock::is_contended() {
	mcs_node *self = &selfs[this];
	return self->next != nullptr;
}

thread_local std::map<mcs_lock*, mcs_lock::mcs_node> mcs_lock::selfs;

}  // namespace locallock
}  // namespace argo

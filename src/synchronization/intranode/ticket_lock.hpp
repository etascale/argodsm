/**
 * @file
 * @brief This file provides a (local) ticket lock for intranode locking in the ArgoDSM system
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_local_ticket_lock_hpp
#define argo_local_ticket_lock_hpp argo_local_ticket_lock_hpp

#include <atomic>

namespace argo {
	namespace locallock {
		/** @brief a global test-and-set lock */
		class ticket_lock {
			private:
				/** @brief Amount of threads wanting the lock */
				std::atomic<int> in_counter;

				/** @brief Amount of threads exiting the lock */
				std::atomic<int> out_counter;

			public:
				/**
				 * @brief constructs a ticket_lock
				 */
				ticket_lock() : in_counter(0), out_counter(0) {}

				/**
				 * @brief take the lock by fetching your ticket and wait untill out_counter matches your ticket
				 */
				void lock() {
					int ticket = in_counter.fetch_add(1, std::memory_order_relaxed);
					while(out_counter.load(std::memory_order_acquire) != ticket);
				}

				/**
				 * @brief release the lock
				 */
				void unlock() {
					out_counter.fetch_add(1, std::memory_order_release);
				}

				/**
				 * @brief Checks if the lock is contended by comparing in vs out counters
				 * @return true it the lock is contended (some thread is waiting to get the lock) false otherwise.
				 */
				bool is_contended(){
					int local_in, local_out;
					local_in = in_counter.load(std::memory_order_relaxed);
					local_out = out_counter.load(std::memory_order_relaxed);
					return (local_in - local_out) > 1;
				}
		};
	} // namespace locallock
} // namespace argo

#endif /* argo_local_ticket_lock_hpp */

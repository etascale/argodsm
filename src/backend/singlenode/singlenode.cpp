/**
 * @file
 * @brief pseudo-backend implemenation for a single node ArgoDSM system
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include "data_distribution/global_ptr.hpp"
#include "signal/signal.hpp"
#include "synchronization/global_tas_lock.hpp"
#include "types/types.hpp"
#include "virtual_memory/virtual_memory.hpp"
#include "../backend.hpp"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <mutex>

namespace vm = argo::virtual_memory;
namespace sig = argo::signal;

/** @brief a lock for atomically executed operations */
std::mutex atomic_op_mutex;

/** @brief a lock for barrier operations */
std::mutex barrier_mutex;

/** @brief a condition variable for barrier operations */
std::condition_variable barrier_cv;

/** @brief a counter for barrier operations */
std::atomic<std::size_t> barrier_counter{0};

/** @brief a flag for in-progress barriers */
std::atomic<bool> barrier_flag{false};

/** @brief scoped locking type */
using lock_guard = std::lock_guard<std::mutex>;

/** @brief the only valid ArgoDSM node id */
const argo::node_id_t my_node_id = 0;

/** @brief the only supported amount of ArgoDSM nodes */
const std::size_t nodes = 1;

/**
 * @brief memory base pointer
 * @deprecated this is to mimic the prototype and mpi backend
 */
char* memory;

/**
 * @brief total memory size in bytes
 * @deprecated this is to mimic the prototype and mpi backend
 */
std::size_t memory_size;

/*First-Touch policy*/
/** @brief holds the owner of a page */
std::uintptr_t *global_owners;
/** @brief allocator offset for the node */
std::size_t owner_offset;

/**
 * @brief a dummy signal handler function
 * @warning this function is not strictly portable because it resets the handler and re-raises the signal
 * @param sig signal number
 * @see check `man sigaction` for additional information
 */
void singlenode_handler(int sig, siginfo_t*, void*) {
	printf("A segfault was encountered in ArgoDSM memory. "
		"This should never happen, as the singlenode backend is in use.\n"
	);
	std::signal(sig, SIG_DFL);
	std::raise(sig);
}
namespace argo {
	namespace backend {

		void init(std::size_t argo_size, std::size_t cache_size){
			/** @todo the cache_size parameter is not needed
			 *        and should not be part of the backend interface */
			(void)(cache_size);
			memory = static_cast<char*>(vm::allocate_mappable(4096, argo_size));
			memory_size = argo_size;
			using namespace data_distribution;
			base_distribution<0>::set_memory_space(nodes, memory, argo_size);
			sig::signal_handler<SIGSEGV>::install_argo_handler(&singlenode_handler);
			/** @note first-touch needs a directory for fetching
			 *        the homenode and offset for an address */
			if (is_first_touch_policy()) {
				/* calculate the directory size and allocate memory */
				owner_offset = 0;
				std::size_t owner_size = 2*(argo_size/4096UL);
				std::size_t owner_size_bytes = owner_size*sizeof(std::size_t);
				owner_size_bytes = (1 + ((owner_size_bytes-1) / 4096UL))*4096UL;
				global_owners = static_cast<std::uintptr_t*>(vm::allocate_mappable(4096UL, owner_size_bytes));
				/* hardcode first-touch directory values so
				   that we perform only load operations */
				for(std::size_t j = 0; j < owner_size; j += 2) {
					global_owners[j] = 0x1UL;
					global_owners[j+1] = j/2 * 4096UL;
				}
			}
		}

		node_id_t node_id() {
			return my_node_id;
		}

		int number_of_nodes() {
			return nodes;
		}

		std::size_t& backing_offset() {
			return owner_offset;
		}

		char* global_base() {
			return memory;
		}

		std::size_t global_size() {
			return memory_size;
		}

		void finalize() {
		}

		void barrier(std::size_t threadcount) {
			/* initially: flag = false */
			std::unique_lock<std::mutex> barrier_lock(barrier_mutex);
			barrier_counter++;
			if(barrier_counter == threadcount) {
				/* avoid progress if some thread is still stuck in the previous barrier */
				barrier_cv.wait(barrier_lock, []{ return !barrier_flag.load(); });
				/* all threads in this barrier now,
				 * flip flag to avoid progress in next barrier */
				barrier_flag = true;
				barrier_cv.notify_all();
			}

			/* wait for flag to flip to ensure all threads have reached this barrier */
			barrier_cv.wait(barrier_lock, []{ return barrier_flag.load(); });

			/* all threads synchronized here. cleanup needed. */

			barrier_counter--;
			if(barrier_counter == 0) {
				/* all threads successfully left the wait call, allow next barrier */
				barrier_flag = false;
				barrier_cv.notify_all(); // other threads + unstick next barrier's final thread
			}

			/** @bug there is a race here when the next barrier does not require
			 *       a thread stuck in this particular wait, which can then be
			 *       delayed until the end of that other barrier call
			 */
			barrier_cv.wait(barrier_lock, []{ return !barrier_flag; });
		}

		template<typename T>
		void broadcast(node_id_t source, T* ptr) {
			(void)source; // source is always node 0
			(void)ptr; // synchronization with self is a no-op
		}

#		include "../explicit_instantiations.inc.cpp"

		void acquire() {
			std::atomic_thread_fence(std::memory_order_acquire);
		}
		void release() {
			std::atomic_thread_fence(std::memory_order_release);
		}
		void _selective_acquire(void* addr, std::size_t size) {
			(void)addr;
			(void)size;
			acquire();	// Selective acquire not actually possible here
		}
		void _selective_release(void* addr, std::size_t size) {
			(void)addr;
			(void)size;
			release();	// Selective release not actually possible here
		}

		namespace atomic {
			void _exchange(global_ptr<void> obj, void* desired,
					std::size_t size, void* output_buffer) {
				lock_guard lock(atomic_op_mutex);
				memcpy(output_buffer, obj.get(), size);
				memcpy(obj.get(), desired, size);
			}

			void _store(global_ptr<void> obj, void* desired, std::size_t size) {
				lock_guard lock(atomic_op_mutex);
				memcpy(obj.get(), desired, size);
			}

			void _store_public_dir(const void* desired,
					const std::size_t size, const std::size_t rank, const std::size_t disp) {
				(void)desired;
				(void)size;
				(void)rank;
				(void)disp;
			}

			void _store_local_dir(const std::size_t desired,
					const std::size_t rank, const std::size_t disp) {
				(void)desired;
				(void)rank;
				(void)disp;
			}

			void _load(
					global_ptr<void> obj, std::size_t size, void* output_buffer) {
				lock_guard lock(atomic_op_mutex);
				memcpy(output_buffer, obj.get(), size);
			}

			/**
			 * @note only load operations are performed on the first-touch
			 *       directory, since the values are hardcoded in the init call
			 */
			void _load_local_dir(void* output_buffer,
					const std::size_t rank, const std::size_t disp) {
				(void)rank;
				*(static_cast<std::size_t*>(output_buffer)) = global_owners[disp];
			}

			void _compare_exchange(global_ptr<void> obj, void* desired,
					std::size_t size, void* expected, void* output_buffer) {
				lock_guard lock(atomic_op_mutex);
				memcpy(output_buffer, obj.get(), size);
				if (memcmp(obj.get(), expected, size) == 0) {
					memcpy(obj.get(), desired, size);
				}
			}

			void _compare_exchange_dir(const void* desired, const void* expected, void* output_buffer,
					const std::size_t size, const std::size_t rank, const std::size_t disp) {
				(void)desired;
				(void)expected;
				(void)output_buffer;
				(void)size;
				(void)rank;
				(void)disp;
			}

			void _fetch_add_int(global_ptr<void> obj, void* value,
					std::size_t size, void* output_buffer) {
				lock_guard lock(atomic_op_mutex);
				memcpy(output_buffer, obj.get(), size);
				// ewwww...
				switch (size) {
					case 1:
						{
							char *ptr = static_cast<char*>(obj.get());
							char *val = static_cast<char*>(value);
							*ptr += *val;
							break;
						}
					case 2:
						{
							int16_t *ptr = static_cast<int16_t*>(obj.get());
							int16_t *val = static_cast<int16_t*>(value);
							*ptr += *val;
							break;
						}
					case 4:
						{
							int32_t *ptr = static_cast<int32_t*>(obj.get());
							int32_t *val = static_cast<int32_t*>(value);
							*ptr += *val;
							break;
						}
					case 8:
						{
							int64_t *ptr = static_cast<int64_t*>(obj.get());
							int64_t *val = static_cast<int64_t*>(value);
							*ptr += *val;
							break;
						}
					default:
						throw std::invalid_argument(
							"Invalid size (must be power either 1, 2, 4 or 8)");
						break;
				}
			}

			void _fetch_add_uint(global_ptr<void> obj, void* value,
					std::size_t size, void* output_buffer) {
				lock_guard lock(atomic_op_mutex);
				memcpy(output_buffer, obj.get(), size);
				// ewwww...
				switch (size) {
					case 1:
						{
							unsigned char *ptr = static_cast<unsigned char*>(obj.get());
							unsigned char *val = static_cast<unsigned char*>(value);
							*ptr += *val;
							break;
						}
					case 2:
						{
							uint16_t *ptr = static_cast<uint16_t*>(obj.get());
							uint16_t *val = static_cast<uint16_t*>(value);
							*ptr += *val;
							break;
						}
					case 4:
						{
							uint32_t *ptr = static_cast<uint32_t*>(obj.get());
							uint32_t *val = static_cast<uint32_t*>(value);
							*ptr += *val;
							break;
						}
					case 8:
						{
							uint64_t *ptr = static_cast<uint64_t*>(obj.get());
							uint64_t *val = static_cast<uint64_t*>(value);
							*ptr += *val;
							break;
						}
					default:
						throw std::invalid_argument(
							"Invalid size (must be power either 1, 2, 4 or 8)");
						break;
				}
			}

			void _fetch_add_float(global_ptr<void> obj, void* value,
					std::size_t size, void* output_buffer) {
				lock_guard lock(atomic_op_mutex);
				memcpy(output_buffer, obj.get(), size);
				// ewwww...
				switch (size) {
					case sizeof(float):
						{
							float *ptr = static_cast<float*>(obj.get());
							float *val = static_cast<float*>(value);
							*ptr += *val;
							break;
						}
					case sizeof(double):
						{
							double *ptr = static_cast<double*>(obj.get());
							double *val = static_cast<double*>(value);
							*ptr += *val;
							break;
						}
					case sizeof(long double):
						{
							long double *ptr = static_cast<long double*>(obj.get());
							long double *val = static_cast<long double*>(value);
							*ptr += *val;
							break;
						}
					default:
						throw std::invalid_argument(
							"Invalid size (must be power either 1, 2, 4 or 8)");
						break;
				}
			}
		} // namespace atomic

	} // namespace backend
} // namespace argo


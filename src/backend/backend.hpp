/**
 * @file
 * @brief This file provides the backend interface
 * @details The backend interface can be implemented independently
 *          for different interconnects or purposes.
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_backend_backend_hpp
#define argo_backend_backend_hpp argo_backend_backend_hpp

#include <cassert>
#include <cstdint>
#include <stdexcept>

#include "../data_distribution/global_ptr.hpp"
#include "../types/types.hpp"

namespace argo {
	/**
	 * @brief namespace for backend functionality
	 * @details The backend functionality in ArgoDSM is all functions that
	 *          depend inherently on the underlying communications system
	 *          used between ArgoDSM nodes. These functions need to be
	 *          implemented separately for each backend.
	 */
	namespace atomic {
		/**
		 * @brief Memory ordering for synchronization operations
		 */
		enum class memory_order {
			relaxed, ///< No synchronization
			// consume,
			acquire, ///< This operation is an acquire operation
			release, ///< This operation is a release opearation
			acq_rel, ///< Release + Acquire
			// seq_cst
		};
	}

	namespace backend {
		/**
		 * @brief initialize backend
		 * @param argo_size the size (in bytes) of the global memory to initialize
		 * @param cache_size the size (in bytes) of the cache used by ArgoDSM
		 * @warning the signature of this function may change
		 * @todo maybe this should be tied better to the concrete memory
		 *       allocated rather than a generic "initialize this size"
		 *       functionality.
		 * @todo the cache_size parameter is currently needed because cache layout
		 *       is defined by the backend, which is wrong design-wise.
		 */
		void init(std::size_t argo_size, std::size_t cache_size);

		/**
		 * @brief get ArgoDSM node ID
		 * @return local ArgoDSM node ID
		 */
		node_id_t node_id();

		/**
		 * @brief get total number of ArgoDSM nodes
		 * @return total number of ArgoDSM nodes
		 */
		node_id_t number_of_nodes();

		/**
		 * @brief get base address of global memory
		 * @return the pointer to the start of the global memory
		 * @deprecated this is an implementation detail of the prototype
		 *             implementation and should not be used in new code.
		 */
		char* global_base();

		/**
		 * @brief get the total amount of global memory
		 * @return the size of the global memory
		 * @deprecated this is an implementation detail of the prototype
		 *             implementation and should not be used in new code.
		 */
		std::size_t global_size();


		/**
		 * @brief global memory space address
		 * @deprecated prototype implementation detail
		 */
		void finalize();

		/** @brief import global_ptr */
		using argo::data_distribution::global_ptr;

		/**
		 * @brief a simple collective barrier
		 * @param threadcount number of threads on each node that go into the barrier
		 */
		void barrier(std::size_t threadcount=1);

		/**
		 * @brief broadcast-style collective synchronization
		 * @tparam T type of synchronized object
		 * @param source the ArgoDSM node holding the authoritative copy
		 * @param ptr pointer to the object to synchronize
		 */
		template<typename T>
		void broadcast(node_id_t source, T* ptr);

		/**
		 * @brief causes a node to self-invalidate its cache,
		 *        and thus getting any updated values on subsequent accesses
		 */
		void acquire();

		/**
		 * @brief causes a node to self-downgrade its cache,
		 *        making all previous writes to be propagated to the homenode,
		 *        visible for nodes calling acquire (or other synchronization primitives)
		 */
		void release();


		/**
		 * The following selective coherence functions are implemented individually
		 * by each backend
		 */

		/**
		 * @brief backend internal type erased function for selective acquire
		 * @param addr pointer to the start of an ArgoDSM memory region
		 * @param size size of the region pointed to by addr
		 * @sa    selective_acquire
		 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
		 */
		void _selective_acquire(void* addr, std::size_t size);

		/**
		 * @brief backend internal type erased function for selective release
		 * @param addr pointer to the start of an ArgoDSM memory region
		 * @param size size of the region pointed to by addr
		 * @sa    selective_release
		 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
		 */
		void _selective_release(void* addr, std::size_t size);

		/**
		 * The following selective coherence functions are generic interfaces to the
		 * actual backend implementations
		 */

		/**
		 * @brief causes a node to self-invalidate the specified pages from its cache,
		 *        and thus getting any updated values on subsequent accesses
		 * @param addr pointer to the start of the memory region to self-invalidate
		 * @param size the size of the memory region pointed to by addr. A size equal
		 *        to zero results in that no pages are selectively acquired.
		 * @note  Selective coherence operations do not uphold the data consistency
		 *        semantics of regular coherence operations and should be used with
		 *        this in mind. Specifically, write ordering with respect to writes
		 *        outside of the selective coherence region is not upheld.
		 */
		template<typename T>
		void selective_acquire(T* addr, std::size_t size) {
			_selective_acquire(static_cast<void*>(addr), size);
		}

		/**
		 * @brief causes a node to self-downgrade specified pages from its cache,
		 *        making all previous writes to be propagated to the homenode,
		 *        visible for nodes calling acquire (or other synchronization primitives)
		 * @param addr the start of the memory region to self-invalidate
		 * @param size the size of the memory region pointed to by addr. A size equal
		 *        to zero results in that no pages are selectively released.
		 * @note  Selective coherence operations do not uphold the data consistency
		 *        semantics of regular coherence operations and should be used with
		 *        this in mind. Specifically, write ordering with respect to writes
		 *        outside of the selective coherence region is not upheld.
		 */
		template<typename T>
		void selective_release(T* addr, std::size_t size) {
			_selective_release(static_cast<void*>(addr), size);
		}

		namespace atomic {
			using namespace argo::atomic;

			/*
			 * The following atomic functions are implemented by each backend in
			 * their respective cpp files
			 */

			/**
			 * @brief Backend internal type erased atomic exchange function
			 * @param obj Pointer to the object whose value should be exchanged
			 * @param desired Pointer to the object that holds the desired value
			 * @param size sizeof(*obj) == sizeof(*desired) == sizeof(output_buffer)
			 * @param output_buffer Pointer to the memory location where the old value of the object should be stored
			 * @sa exchange
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _exchange(global_ptr<void> obj, void* desired,
				std::size_t size, void* output_buffer);

			/**
			 * @brief Backend internal type erased atomic store function
			 * @param obj Pointer to the object whose value should be exchanged
			 * @param desired Pointer to the object that holds the desired value
			 * @param size sizeof(*obj) == sizeof(*desired) == sizeof(output_buffer)
			 * @sa store
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _store(global_ptr<void> obj, void* desired, std::size_t size);

			/**
			 * @brief Backend internal type erased atomic store function
			 * @param desired Pointer to the object that holds the desired value
			 * @param size == sizeof(*desired)
			 * @param rank Rank of target window to which operation will be performed
			 * @param disp Displacement from start of window to beginning of target buffer
			 * @sa store to public window section
			 * @note Implementation-specific function for the first-touch data distribution.
			 *       It is used to perform stores on the owners_dir_window window.
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _store_public_owners_dir(const void* desired,
				const std::size_t size, const std::size_t rank, const std::size_t disp);

			/**
			 * @brief Backend internal type erased atomic store function
			 * @param desired The desired value to be stored
			 * @param rank Rank of target window to which operation will be performed
			 * @param disp Displacement from start of window to beginning of target buffer
			 * @sa store to private window section
			 * @note Implementation-specific function for the first-touch data distribution.
			 *       It is used to perform stores on the owners_dir_window window.
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _store_local_owners_dir(const std::size_t* desired,
				const std::size_t rank, const std::size_t disp);

			/**
			 * @brief Backend internal type erased atomic store function
			 * @param desired The desired value to be stored
			 * @param rank Rank of target window to which operation will be performed
			 * @param disp Displacement from start of window to beginning of target buffer
			 * @sa store to private window section
			 * @note Implementation-specific function for the first-touch data distribution.
			 *       It is used to perform stores on the offsets_tbl_window window.
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _store_local_offsets_tbl(const std::size_t desired,
				const std::size_t rank, const std::size_t disp);

			/**
			 * @brief Backend internal type erased atomic store function
			 * @param obj Pointer to the object whose value should be exchanged
			 * @param size sizeof(*obj) == sizeof(output_buffer)
			 * @param output_buffer Pointer to the memory location where the value of the object should be stored
			 * @sa load
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _load(
				global_ptr<void> obj, std::size_t size, void* output_buffer);

			/**
			 * @brief Backend internal type erased atomic load function
			 * @param output_buffer Pointer to the memory location where the value of the object should be stored
			 * @param size == sizeof(*output_buffer)
			 * @param rank Rank of target window to which operation will be performed
			 * @param disp Displacement from start of window to beginning of target buffer
			 * @sa load from public window section
			 * @note Implementation-specific function for the first-touch data distribution.
			 *       It is used to perform loads on the owners_dir_window window.
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _load_public_owners_dir(void* output_buffer,
				const std::size_t size, const std::size_t rank, const std::size_t disp);

			/**
			 * @brief Backend internal type erased atomic load function
			 * @param output_buffer Pointer to the memory location where the value of the object should be stored
			 * @param rank Rank of target window to which operation will be performed
			 * @param disp Displacement from start of window to beginning of target buffer
			 * @sa load from private window section
			 * @note Implementation-specific function for the first-touch data distribution.
			 *       It is used to perform loads on the owners_dir_window window.
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _load_local_owners_dir(void* output_buffer,
				const std::size_t rank, const std::size_t disp);
				
			/**
			 * @brief Backend internal type erased atomic load function
			 * @param output_buffer Pointer to the memory location where the value of the object should be stored
			 * @param rank Rank of target window to which operation will be performed
			 * @param disp Displacement from start of window to beginning of target buffer
			 * @sa load from private window section
			 * @note Implementation-specific function for the first-touch data distribution.
			 *       It is used to perform loads on the offsets_tbl_window window.
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _load_local_offsets_tbl(void* output_buffer,
				const std::size_t rank, const std::size_t disp);

			/**
			 * @brief Backend internal type erased atomic CAS function
			 * @param obj Pointer to the object whose value should be exchanged
			 * @param desired Pointer to the object that holds the desired value
			 * @param expected Pointer to the object that holds the expected value
			 * @param size sizeof(*obj) == sizeof(*desired) == sizeof(*expected) == sizeof(output_buffer)
			 * @param output_buffer Pointer to the memory location where the old value of the object should be stored
			 * @sa compare_exchange
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _compare_exchange(global_ptr<void> obj, void* desired,
				std::size_t size, void* expected, void* output_buffer);

			/**
			 * @brief Backend internal type erased atomic CAS function
			 * @param desired Pointer to the object that holds the desired value
			 * @param expected Pointer to the object that holds the expected value
			 * @param output_buffer Pointer to the memory location where the old value of the object should be stored
			 * @param size == sizeof(*desired) == sizeof(*expected) == sizeof(*output_buffer)
			 * @param rank Rank of target window to which operation will be performed
			 * @param disp Displacement from start of window to beginning of target buffer
			 * @sa compare_exchange
			 * @note Implementation-specific function for the first-touch data distribution.
			 *       It is used to perform CAS on the owners_dir_window window.
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _compare_exchange_owners_dir(const void* desired, const void* expected, void* output_buffer,
				const std::size_t size, const std::size_t rank, const std::size_t disp);

			/**
			 * @brief Backend internal type erased atomic CAS function
			 * @param desired Pointer to the object that holds the desired value
			 * @param expected Pointer to the object that holds the expected value
			 * @param output_buffer Pointer to the memory location where the old value of the object should be stored
			 * @param size == sizeof(*desired) == sizeof(*expected) == sizeof(*output_buffer)
			 * @param rank Rank of target window to which operation will be performed
			 * @param disp Displacement from start of window to beginning of target buffer
			 * @sa compare_exchange
			 * @note Implementation-specific function for the first-touch data distribution.
			 *       It is used to perform CAS on the offsets_tbl_window window.
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _compare_exchange_offsets_tbl(const void* desired, const void* expected, void* output_buffer,
				const std::size_t size, const std::size_t rank, const std::size_t disp);

			/**
			 * @brief Backend internal type erased atomic (post)increment function for signed integers
			 * @param obj Pointer to the object whose value should be exchanged
			 * @param value Pointer to the object that holds the desired value
			 * @param size sizeof(*obj) == sizeof(*desired) == sizeof(output_buffer)
			 * @param output_buffer Pointer to the memory location where the old value of the object should be stored
			 * @sa exchange
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _fetch_add_int(global_ptr<void> obj, void* value, std::size_t size,
				void* output_buffer);
			/**
			 * @brief Backend internal type erased atomic (post)increment function for unsigned integers
			 * @param obj Pointer to the object whose value should be exchanged
			 * @param value Pointer to the object that holds the desired value
			 * @param size sizeof(*obj) == sizeof(*desired) == sizeof(output_buffer)
			 * @param output_buffer Pointer to the memory location where the old value of the object should be stored
			 * @sa exchange
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _fetch_add_uint(global_ptr<void> obj, void* value, std::size_t size,
				void* output_buffer);
			/**
			 * @brief Backend internal type erased atomic (post)increment function for floating point numbers
			 * @param obj Pointer to the object whose value should be exchanged
			 * @param value Pointer to the object that holds the desired value
			 * @param size sizeof(*obj) == sizeof(*desired) == sizeof(output_buffer)
			 * @param output_buffer Pointer to the memory location where the old value of the object should be stored
			 * @sa exchange
			 * @warning For internal use only - DO NOT USE UNLESS YOU KNOW WHAT YOU ARE DOING
			 */
			void _fetch_add_float(global_ptr<void> obj, void* value, std::size_t size,
				void* output_buffer);

			/**
			 * The following atomic functions are generic interfaces to the
			 * actual backend implementations
			 */

			/**
			 * @brief atomic swap operation on a global address
			 * @param obj pointer to the global object to swap
			 * @param desired the new value of the global object
			 * @param order Memory synchronization ordering for this operation
			 * @tparam T The type of the object to operate upon
			 * @tparam U The type of the object to exchange with
			 * @return the old value of the global object
			 *
			 * This function will atomically exchange the old value of the given
			 * object with the new one and return the old one.
			 */
			template<typename T, typename U>
			T exchange(global_ptr<T> obj, U desired, memory_order order = memory_order::acq_rel) {
				T out_buffer;
				static_assert(std::is_convertible<U, T>::value,
					"It is not possible to implicitly convert \'desired\' to an"
					" object of type T.");
				T desired_buffer(desired);
#if __GNUC__ >= 5 || __clang__
				static_assert(
					std::is_trivially_copy_assignable<T>::value,
					"T must be trivially copy assignable"
				);
#endif // __GNUC__ >= 5 || __clang__

				if (order == memory_order::acq_rel || order == memory_order::release)
					release();

				_exchange(global_ptr<void>(obj), &desired_buffer, sizeof(T), &out_buffer);

				if (order == memory_order::acq_rel || order == memory_order::acquire)
					acquire();

				return out_buffer;
			}

			/**
			 * @brief Atomic store operation on a global address
			 * @tparam T type of object to write
			 * @param obj pointer to the object to write to
			 * @param desired value to store in the object
			 * @param order Memory synchronization ordering for this operation
			 * @tparam T The type of the object to operate upon
			 * @tparam U The type of the object to store
			 *
			 * This function will atomically store the given value into the
			 * given memory location.
			 */
			template<typename T, typename U>
			void store(global_ptr<T> obj, U desired, memory_order order = memory_order::release) {
				static_assert(std::is_convertible<U, T>::value,
					"It is not possible to implicitly convert \'desired\' to an"
					" object of type T.");
				T desired_buffer(desired);
#if __GNUC__ >= 5 || __clang__
				static_assert(
					std::is_trivially_copy_assignable<T>::value,
					"T must be trivially copy assignable"
				);
#endif // __GNUC__ >= 5 || __clang__

				if (order == memory_order::acq_rel || order == memory_order::release)
					release();

				_store(global_ptr<void>(obj), &desired_buffer, sizeof(T));

				if (order == memory_order::acq_rel || order == memory_order::acquire)
					acquire();
			}

			/**
			 * @brief Atomic load operation on a global address
			 * @param obj Pointer to the global object to swap
			 * @param order Memory synchronization ordering for this operation
			 * @tparam T The type of the object to operate upon
			 * @return The value of the global object
			 *
			 * This function will atomically load the value from the given
			 * memory location.
			 */
			template<typename T>
			T load(global_ptr<T> obj, memory_order order = memory_order::acquire) {
				T out_buffer;
#if __GNUC__ >= 5 || __clang__
				static_assert(
					std::is_trivially_copy_assignable<T>::value,
					"T must be trivially copy assignable"
				);
#endif // __GNUC__ >= 5 || __clang__

				if (order == memory_order::acq_rel || order == memory_order::release)
					release();

				_load(global_ptr<void>(obj), sizeof(T), &out_buffer);

				if (order == memory_order::acq_rel || order == memory_order::acquire)
					acquire();

				return out_buffer;
			}

			/**
			 * @brief Atomic CAS operation on a global address
			 * @param obj Pointer to the global object to swap
			 * @param expected The value to compare against
			 * @param desired The value to set the object to
			 * @param order Memory synchronization ordering for this operation
			 * @tparam T The type of the object to operate upon
			 * @tparam U The type of the expected object
			 * @tparam U The type of the desired object
			 * @return true if successful, false otherwise
			 *
			 * This function will atomically swap the old (expected) value of
			 * the object with the new (desired) one, but only if the actual
			 * value of the object matches the expected one.
			 */
			template <typename T, typename U, typename V>
			bool compare_exchange(global_ptr<T> obj, U expected, V desired,
				memory_order order = memory_order::acq_rel) {
				T out_buffer;
				static_assert(std::is_convertible<U, T>::value,
					"It is not possible to implicitly convert \'expected\' to an"
					" object of type T.");
				static_assert(std::is_convertible<V, T>::value,
					"It is not possible to implicitly convert \'desired\' to an"
					" object of type T.");
				T expected_buffer(expected);
				T desired_buffer(desired);
#if __GNUC__ >= 5 || __clang__
				static_assert(
					std::is_trivially_copy_assignable<T>::value,
					"T must be trivially copy assignable"
				);
#endif // __GNUC__ >= 5 || __clang__

				if (order == memory_order::acq_rel || order == memory_order::release)
					release();

				//XXX: Is this strong or weak? MPI doesn't say anything
				_compare_exchange(global_ptr<void>(obj), &desired_buffer, sizeof(T),
					&expected_buffer, &out_buffer);

				if (order == memory_order::acq_rel || order == memory_order::acquire)
					acquire();

				return out_buffer == expected_buffer;
			}

			/**
			 * @brief Atomic fetch and add operation on a global address
			 * @param obj Pointer to the global object to fetch and add to
			 * @param value The value to add to the object
			 * @param order Memory synchronization ordering for this operation
			 * @tparam T The type of the object to operate upon
			 * @tparam U The type of the value object
			 * @return The value of the object BEFORE the add operation
			 *
			 * This function will perform an atomic (*obj)+=(*value) operation.
			 */
			template <typename T, typename U>
			T fetch_add(global_ptr<T> obj, U value, memory_order order = memory_order::acq_rel) {
				T out_buffer;
				// We need a buffer with the same size as T
				static_assert(std::is_convertible<U, T>::value,
					"It is not possible to implicitly convert \'desired\' to an"
					" object of type T.");
				T value_buffer(value);
#if __GNUC__ >= 5 || __clang__
				static_assert(
					std::is_trivially_copy_assignable<T>::value,
					"T must be trivially copy assignable"
				);
#endif // __GNUC__ >= 5 || __clang__

				if (order == memory_order::acq_rel || order == memory_order::release)
					release();

				// The order is important here, as floats are signed as well
				if (std::is_floating_point<T>::value)
					_fetch_add_float(global_ptr<void>(obj), &value_buffer, sizeof(T), &out_buffer);
				else if (std::is_unsigned<T>::value)
					_fetch_add_uint(global_ptr<void>(obj), &value_buffer, sizeof(T), &out_buffer);
				else if (std::is_signed<T>::value)
					_fetch_add_int(global_ptr<void>(obj), &value_buffer, sizeof(T), &out_buffer);
				else
					throw std::invalid_argument("Invalid type V for value");

				if (order == memory_order::acq_rel || order == memory_order::acquire)
					acquire();

				return out_buffer;
			}

			/**
			 * @brief Atomic fetch and add operation for pointers on a global address
			 * @param obj Pointer to the global object to fetch and add to
			 * @param value The value to add to the object
			 * @param order Memory synchronization ordering for this operation
			 * @tparam T The type of the pointer to operate upon
			 * @tparam U The type of the value object
			 * @return The value of the pointer BEFORE the add operation
			 *
			 * This function will perform an atomic (*obj)+=(*value) operation.
			 * This is an overload for pointer types, as pointer arithmetic
			 * works differently than normal integer and floating point
			 * arithmetic.
			 */
			template <typename T, typename U>
			T* fetch_add(global_ptr<T*> obj, U value, memory_order order = memory_order::acq_rel) {
				T* out_buffer;
				// We need a buffer with the same size as the pointer
				void *value_buffer = reinterpret_cast<char*>(value * sizeof(T));

				if (order == memory_order::acq_rel || order == memory_order::release)
					release();

				// Do the operation
				_fetch_add_uint(global_ptr<void>(obj), &value_buffer, sizeof(T*), &out_buffer);

				if (order == memory_order::acq_rel || order == memory_order::acquire)
					acquire();

				return out_buffer;
			}
		} // namespace atomic
	} // namespace backend
} // namespace argo

#endif /* argo_backend_backend_hpp */

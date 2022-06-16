/**
 * @file
 * @brief MPI backend implementation
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

// C headers
#include <mpi.h>
#include <semaphore.h>
// C++ headers
#include <algorithm>
#include <atomic>
#include <type_traits>

#include "swdsm.h"
#include "../backend.hpp"

/**
 * @brief Returns an MPI integer type that exactly matches in size the argument given
 *
 * @param size The size of the datatype to be returned
 * @return An MPI datatype with MPI_Type_size == size
 * @todo Either remove the 1/2 byte case entirely, or return at least 4 bytes and
 * enforce 4 byte alignment of any MPI operations using the returned type.
 */
static MPI_Datatype fitting_mpi_int(std::size_t size) {
	MPI_Datatype t_type;
	using namespace argo;

	switch (size) {
	// Until UCX supports 1/2 byte atomics, they must be disabled
	//case 1:
	//	t_type = MPI_INT8_T;
	//	break;
	//case 2:
	//	t_type = MPI_INT16_T;
	//	break;
	case 4:
		t_type = MPI_INT32_T;
		break;
	case 8:
		t_type = MPI_INT64_T;
		break;
	default:
		throw std::invalid_argument(
			"Invalid size (must be either 4 or 8)");
		break;
	}

	return t_type;
}

/**
 * @brief Returns an MPI unsigned integer type that exactly matches in size the argument given
 *
 * @param size The size of the datatype to be returned
 * @return An MPI datatype with MPI_Type_size == size
 * @todo Either remove the 1/2 byte case entirely, or return at least 4 bytes and
 * enforce 4 byte alignment of any MPI operations using the returned type.
 */
static MPI_Datatype fitting_mpi_uint(std::size_t size) {
	MPI_Datatype t_type;
	using namespace argo;

	switch (size) {
	// Until UCX supports 1/2 byte atomics, they must be disabled
	//case 1:
	//	t_type = MPI_UINT8_T;
	//	break;
	//case 2:
	//	t_type = MPI_UINT16_T;
	//	break;
	case 4:
		t_type = MPI_UINT32_T;
		break;
	case 8:
		t_type = MPI_UINT64_T;
		break;
	default:
		throw std::invalid_argument(
			"Invalid size (must be either 4 or 8)");
		break;
	}

	return t_type;
}

/**
 * @brief Returns an MPI floating point type that exactly matches in size the argument given
 *
 * @param size The size of the datatype to be returned
 * @return An MPI datatype with MPI_Type_size == size
 */
static MPI_Datatype fitting_mpi_float(std::size_t size) {
	MPI_Datatype t_type;
	using namespace argo;

	switch (size) {
	case 4:
		t_type = MPI_FLOAT;
		break;
	case 8:
		t_type = MPI_DOUBLE;
		break;
	// Until UCX supports 16 byte atomics, it must be disabled
	//case 16:
	//	t_type = MPI_LONG_DOUBLE;
	//	break;
	default:
		throw std::invalid_argument(
			"Invalid size (must be power either 4, 8)");
		break;
	}

	return t_type;
}

namespace argo {
	namespace backend {
		void init(std::size_t argo_size, std::size_t cache_size) {
			argo_initialize(argo_size, cache_size);
		}

		node_id_t node_id() {
			return argo_get_nid();
		}

		node_id_t number_of_nodes() {
			return argo_get_nodes();
		}

		char* global_base() {
			return static_cast<char*>(argo_get_global_base());
		}

		std::size_t global_size() {
			return argo_get_global_size();
		}

		bool is_cached(void* addr) {
			return _is_cached(reinterpret_cast<std::size_t>(addr));
		}

		void reset_stats() {
			argo_reset_stats();
		}

		void finalize() {
			argo_finalize();
		}

		void reset_coherence() {
			argo_reset_coherence();
		}

		void barrier(std::size_t tc, upgrade_type upgrade) {
			swdsm_argo_barrier(tc, upgrade);
		}

		template<typename T>
		void broadcast(node_id_t source, T* ptr) {
			MPI_Bcast(static_cast<void*>(ptr), sizeof(T), MPI_BYTE, source, workcomm);
		}

		void acquire() {
			argo_acquire();
			std::atomic_thread_fence(std::memory_order_acquire);
		}
		void release() {
			std::atomic_thread_fence(std::memory_order_release);
			argo_release();
		}

#include "../explicit_instantiations.inc.cpp"

		namespace atomic {
			void _exchange(global_ptr<void> obj, void* desired,
					std::size_t size, void* output_buffer) {
				MPI_Datatype t_type = fitting_mpi_int(size);
				// Perform the exchange operation
				std::size_t win_index = get_data_win_index(obj.offset());
				std::size_t win_offset = get_data_win_offset(obj.offset());
				mpi_lock_data[win_index][obj.node()].lock(MPI_LOCK_EXCLUSIVE, obj.node(), data_windows[win_index][obj.node()]);
				MPI_Fetch_and_op(desired, output_buffer, t_type, obj.node(), win_offset, MPI_REPLACE, data_windows[win_index][obj.node()]);
				mpi_lock_data[win_index][obj.node()].unlock(obj.node(), data_windows[win_index][obj.node()]);
			}

			void _store(global_ptr<void> obj, void* desired, std::size_t size) {
				MPI_Datatype t_type = fitting_mpi_int(size);
				// Perform the store operation
				std::size_t win_index = get_data_win_index(obj.offset());
				std::size_t win_offset = get_data_win_offset(obj.offset());
				mpi_lock_data[win_index][obj.node()].lock(MPI_LOCK_EXCLUSIVE, obj.node(), data_windows[win_index][obj.node()]);
				MPI_Put(desired, 1, t_type, obj.node(), win_offset, 1, t_type, data_windows[win_index][obj.node()]);
				mpi_lock_data[win_index][obj.node()].unlock(obj.node(), data_windows[win_index][obj.node()]);
			}

			void _store_public_owners_dir(const void* desired,
					const std::size_t size, const std::size_t count,
					const std::size_t rank, const std::size_t disp) {
				MPI_Datatype t_type = fitting_mpi_int(size);
				// Perform the store operation
				MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, owners_dir_window);
				MPI_Put(desired, count, t_type, rank, disp, count, t_type, owners_dir_window);
				MPI_Win_unlock(rank, owners_dir_window);
			}

			void _store_local_owners_dir(const std::size_t* desired,
					const std::size_t count, const std::size_t rank, const std::size_t disp) {
				// Perform the store operation
				MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, owners_dir_window);
				std::copy(desired, desired + count, &global_owners_dir[disp]);
				MPI_Win_unlock(rank, owners_dir_window);
			}

			void _store_local_offsets_tbl(const std::size_t desired,
					const std::size_t rank, const std::size_t disp) {
				// Perform the store operation
				MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, offsets_tbl_window);
				global_offsets_tbl[disp] = desired;
				MPI_Win_unlock(rank, offsets_tbl_window);
			}

			void _load(global_ptr<void> obj, std::size_t size,
					void* output_buffer) {
				MPI_Datatype t_type = fitting_mpi_int(size);
				// Perform the store operation
				std::size_t win_index = get_data_win_index(obj.offset());
				std::size_t win_offset = get_data_win_offset(obj.offset());
				mpi_lock_data[win_index][obj.node()].lock(MPI_LOCK_SHARED, obj.node(), data_windows[win_index][obj.node()]);
				MPI_Get(output_buffer, 1, t_type, obj.node(), win_offset, 1, t_type, data_windows[win_index][obj.node()]);
				mpi_lock_data[win_index][obj.node()].unlock(obj.node(), data_windows[win_index][obj.node()]);
			}

			void _load_public_owners_dir(void* output_buffer,
					const std::size_t size, const std::size_t count,
					const std::size_t rank, const std::size_t disp) {
				MPI_Datatype t_type = fitting_mpi_int(size);
				// Perform the load operation
				MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, owners_dir_window);
				MPI_Get(output_buffer, count, t_type, rank, disp, count, t_type, owners_dir_window);
				MPI_Win_unlock(rank, owners_dir_window);
			}

			void _load_local_owners_dir(void* output_buffer,
					const std::size_t count, const std::size_t rank, const std::size_t disp) {
				// Perform the load operation
				MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, owners_dir_window);
				std::copy(&global_owners_dir[disp],
						&global_owners_dir[disp+count],
						static_cast<std::size_t*>(output_buffer));
				MPI_Win_unlock(rank, owners_dir_window);
			}

			void _load_local_offsets_tbl(void* output_buffer,
					const std::size_t rank, const std::size_t disp) {
				// Perform the load operation
				MPI_Win_lock(MPI_LOCK_SHARED, rank, 0, offsets_tbl_window);
				*(static_cast<std::size_t*>(output_buffer)) = global_offsets_tbl[disp];
				MPI_Win_unlock(rank, offsets_tbl_window);
			}

			void _compare_exchange(global_ptr<void> obj, void* desired,
					std::size_t size, void* expected, void* output_buffer) {
				MPI_Datatype t_type = fitting_mpi_int(size);
				// Perform the store operation
				std::size_t win_index = get_data_win_index(obj.offset());
				std::size_t win_offset = get_data_win_offset(obj.offset());
				mpi_lock_data[win_index][obj.node()].lock(MPI_LOCK_EXCLUSIVE, obj.node(), data_windows[win_index][obj.node()]);
				MPI_Compare_and_swap(desired, expected, output_buffer, t_type, obj.node(), win_offset, data_windows[win_index][obj.node()]);
				mpi_lock_data[win_index][obj.node()].unlock(obj.node(), data_windows[win_index][obj.node()]);
			}

			void _compare_exchange_owners_dir(const void* desired, const void* expected, void* output_buffer,
					const std::size_t size, const std::size_t rank, const std::size_t disp) {
				MPI_Datatype t_type = fitting_mpi_int(size);
				// Perform the compare-and-swap operation
				MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, owners_dir_window);
				MPI_Compare_and_swap(desired, expected, output_buffer, t_type, rank, disp, owners_dir_window);
				MPI_Win_unlock(rank, owners_dir_window);
			}

			void _compare_exchange_offsets_tbl(const void* desired, const void* expected, void* output_buffer,
					const std::size_t size, const std::size_t rank, const std::size_t disp) {
				MPI_Datatype t_type = fitting_mpi_int(size);
				// Perform the compare-and-swap operation
				MPI_Win_lock(MPI_LOCK_EXCLUSIVE, rank, 0, offsets_tbl_window);
				MPI_Compare_and_swap(desired, expected, output_buffer, t_type, rank, disp, offsets_tbl_window);
				MPI_Win_unlock(rank, offsets_tbl_window);
			}

			/**
			 * @brief Atomic fetch&add for the MPI backend (for internal usage)
			 *
			 * This function requires the correct MPI type to work. Usually, you
			 * want to use the three _fetch_add_{int,uint,float} functions
			 * instead, which determine the correct type themselves and then
			 * call this function
			 *
			 * @param obj The pointer to the memory location to modify
			 * @param value Pointer to the value to add
			 * @param t_type MPI type of the object, value, and output buffer
			 * @param output_buffer Location to store the return value
			 */
			void _fetch_add(global_ptr<void> obj, void* value,
					MPI_Datatype t_type, void* output_buffer) {
				// Perform the exchange operation
				std::size_t win_index = get_data_win_index(obj.offset());
				std::size_t win_offset = get_data_win_offset(obj.offset());
				mpi_lock_data[win_index][obj.node()].lock(MPI_LOCK_EXCLUSIVE, obj.node(), data_windows[win_index][obj.node()]);
				MPI_Fetch_and_op(value, output_buffer, t_type, obj.node(), win_offset, MPI_SUM, data_windows[win_index][obj.node()]);
				mpi_lock_data[win_index][obj.node()].unlock(obj.node(), data_windows[win_index][obj.node()]);
			}

			void _fetch_add_int(global_ptr<void> obj, void* value,
					std::size_t size, void* output_buffer) {
				MPI_Datatype t_type = fitting_mpi_int(size);
				_fetch_add(obj, value, t_type, output_buffer);
			}

			void _fetch_add_uint(global_ptr<void> obj, void* value,
					std::size_t size, void* output_buffer) {
				MPI_Datatype t_type = fitting_mpi_uint(size);
				_fetch_add(obj, value, t_type, output_buffer);
			}

			void _fetch_add_float(global_ptr<void> obj, void* value,
					std::size_t size, void* output_buffer) {
				MPI_Datatype t_type = fitting_mpi_float(size);
				_fetch_add(obj, value, t_type, output_buffer);
			}
		} // namespace atomic
	} // namespace backend
} // namespace argo

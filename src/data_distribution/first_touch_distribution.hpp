/**
 * @file
 * @brief This file implements the first-touch data distribution
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef ARGODSM_SRC_DATA_DISTRIBUTION_FIRST_TOUCH_DISTRIBUTION_HPP_
#define ARGODSM_SRC_DATA_DISTRIBUTION_FIRST_TOUCH_DISTRIBUTION_HPP_

#include <mutex>
#include <string>

#include "base_distribution.hpp"

/**
 * @brief The number of std::size_t's used to represent a single entry
 * in the global_owners_dir
 */
constexpr std::size_t single_entry_size = 1;

/**
 * @brief The number of single_entry used to classify an ArgoDSM page's
 * ownership in the global_owners_dir
 * @note This number derives from the definition of the global_owners_dir
 * in the MPI backend. Changes to this must be applied in both locations
 * and should probably be centralized at some point.
 */
constexpr std::size_t kPageInfoSize = single_entry_size * 3;

/**
 * @note backend.hpp is not included here as it includes global_ptr.hpp,
 *       which means that the data distribution definitions precede the
 *       backend definitions, and that is why we need to forward here.
 */
namespace argo {
/* forward argo::backend function declarations */
namespace backend {

node_id_t node_id();

/* forward argo::backend::atomic function declarations */
namespace atomic {

void _store_public_owners_dir(const void* desired,
	const std::size_t size, const std::size_t count,
	const std::size_t rank, const std::size_t disp);
void _store_local_owners_dir(const std::size_t* desired,
	const std::size_t count, const std::size_t rank,
	const std::size_t disp);
void _store_local_offsets_tbl(const std::size_t desired,
	const std::size_t rank, const std::size_t disp);
void _load_public_owners_dir(void* output_buffer,
	const std::size_t size, const std::size_t count,
	const std::size_t rank, const std::size_t disp);
void _load_local_owners_dir(void* output_buffer,
	const std::size_t count, const std::size_t rank,
	const std::size_t disp);
void _load_local_offsets_tbl(void* output_buffer,
	const std::size_t rank, const std::size_t disp);
void _compare_exchange_owners_dir(const void* desired,
	const void* expected, void* output_buffer,
	const std::size_t size, const std::size_t rank,
	const std::size_t disp);
void _compare_exchange_offsets_tbl(const void* desired,
	const void* expected, void* output_buffer,
	const std::size_t size, const std::size_t rank,
	const std::size_t disp);

}  // namespace atomic
}  // namespace backend
}  // namespace argo

/** @brief tiny macro to find if all the values of `page_info` are equal to a specific value */
#define is_all_equal_to(arr, val) (((arr[0] == val) && (arr[1] == val) && (arr[2] == val)) ? 1 : 0)
/** @brief tiny macro to find if any of the values of `page_info` is equal to a specific value */
#define is_one_equal_to(arr, val) (((arr[0] == val) || (arr[1] == val) || (arr[2] == val)) ? 1 : 0)

/** @brief error message string */
static const std::string msg_first_touch_fail = "ArgoDSM failed to find a backing node. Please report a bug.";

namespace argo {
namespace data_distribution {
/**
 * @brief the first-touch data distribution
 * @details gives ownership of a page to the node that first touched it.
 * @note if a node's backing store size is not sufficient, it passes the
 *       ownership of the page to a node that can host it.
 */
template<int instance>
class first_touch_distribution : public base_distribution<instance> {
	private:
		/**
		 * @brief try and claim ownership of a page
		 * @param addr address in the global address space
		 */
		static void first_touch(const std::size_t& addr);
		/**
		 * @brief perform the necessary directory actions
		 * @param addr address in the global address space
		 * @param do_first_touch attempt to first touch if true
		 */
		static void update_dirs(const std::size_t& addr, bool do_first_touch = true);
		/** @brief protects the owners directory */
		std::mutex owners_mutex;

	public:
		virtual node_id_t peek_homenode(char* const ptr) {
			std::size_t page_info[kPageInfoSize];
			node_id_t homenode = invalid_node_id;
			static const std::size_t rank = argo::backend::node_id();
			static const std::size_t global_null = base_distribution<instance>::total_size + 1;
			const std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
			const std::size_t owners_dir_window_index = kPageInfoSize * (addr / granularity);

			std::unique_lock<std::mutex> def_lock(owners_mutex, std::defer_lock);
			def_lock.lock();
			/* handle all the necessary directory actions for the global address */
			update_dirs(addr, false);
			/* spin in case the values other than `ownership` have not been reflected to the local window */
			do {
				argo::backend::atomic::_load_local_owners_dir(&page_info, kPageInfoSize, rank, owners_dir_window_index);
			} while (page_info[2] != global_null && page_info[0] == global_null);
			def_lock.unlock();

			/* Update the homenode if we found one */
			if(page_info[0] != global_null) {
				homenode = page_info[0];
			}

			if(homenode >= static_cast<node_id_t>(base_distribution<instance>::nodes)) {
				std::cerr << msg_fetch_homenode_fail << std::endl;
				throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_fetch_homenode_fail);
			}
			return homenode;
		}

		virtual node_id_t homenode(char* const ptr) {
			node_id_t homenode = invalid_node_id;
			static const std::size_t rank = argo::backend::node_id();
			static const std::size_t global_null = base_distribution<instance>::total_size + 1;
			const std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
			const std::size_t owners_dir_window_index = kPageInfoSize * (addr / granularity);

			std::unique_lock<std::mutex> def_lock(owners_mutex, std::defer_lock);
			def_lock.lock();
			/* handle all the necessary directory actions for the global address */
			update_dirs(addr);
			/* spin in case the values other than `ownership` have not been reflected to the local window */
			do {
				argo::backend::atomic::_load_local_owners_dir(&homenode,
						single_entry_size, rank, owners_dir_window_index);
			} while (homenode == static_cast<node_id_t>(global_null));
			def_lock.unlock();

			if(homenode >= static_cast<node_id_t>(base_distribution<instance>::nodes)) {
				std::cerr << msg_fetch_homenode_fail << std::endl;
				throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_fetch_homenode_fail);
			}
			return homenode;
		}

		virtual std::size_t peek_local_offset(char* const ptr) {
			std::size_t page_info[kPageInfoSize];
			std::size_t offset = invalid_offset;
			static const std::size_t rank = argo::backend::node_id();
			static const std::size_t global_null = base_distribution<instance>::total_size + 1;
			const std::size_t drift = (ptr - base_distribution<instance>::start_address) % granularity;
			const std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
			const std::size_t owners_dir_window_index = kPageInfoSize * (addr / granularity);

			std::unique_lock<std::mutex> def_lock(owners_mutex, std::defer_lock);
			def_lock.lock();
			/* handle all the necessary directory actions for the global address */
			update_dirs(addr, false);
			/* spin in case the values other than `ownership` have not been reflected to the local window */
			do {
				argo::backend::atomic::_load_local_owners_dir(&page_info, kPageInfoSize, rank, owners_dir_window_index);
			} while (page_info[2] != global_null && page_info[1] == global_null);
			def_lock.unlock();

			/* Update the offset if we found one */
			if(page_info[1] != global_null) {
				offset = page_info[1] + drift;
			}
			if(offset != invalid_offset &&
				offset >= base_distribution<instance>::size_per_node) {
				std::cerr << msg_fetch_offset_fail << std::endl;
				throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_fetch_offset_fail);
			}
			return offset;
		}

		virtual std::size_t local_offset(char* const ptr) {
			std::size_t offset = invalid_offset;
			static const std::size_t rank = argo::backend::node_id();
			static const std::size_t global_null = base_distribution<instance>::total_size + 1;
			const std::size_t drift = (ptr - base_distribution<instance>::start_address) % granularity;
			const std::size_t addr = (ptr - base_distribution<instance>::start_address) / granularity * granularity;
			const std::size_t owners_dir_window_index = kPageInfoSize * (addr / granularity);

			std::unique_lock<std::mutex> def_lock(owners_mutex, std::defer_lock);
			def_lock.lock();
			/* handle all the necessary directory actions for the global address */
			update_dirs(addr);
			/* spin in case the values other than `ownership` have not been reflected to the local window */
			do {
				argo::backend::atomic::_load_local_owners_dir(&offset,
						single_entry_size, rank, owners_dir_window_index+1);
			} while (offset == global_null);
			def_lock.unlock();
			offset += drift;

			if(offset >= base_distribution<instance>::size_per_node) {
				std::cerr << msg_fetch_offset_fail << std::endl;
				throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_fetch_offset_fail);
			}
			return offset;
		}
};

template<int instance>
void first_touch_distribution<instance>::update_dirs(const std::size_t& addr,
		const bool do_first_touch) {
	std::size_t ownership;
	static const std::size_t rank = argo::backend::node_id();
	static const std::size_t global_null = base_distribution<instance>::total_size + 1;
	const std::size_t owners_dir_window_index = kPageInfoSize * (addr / granularity);
	const std::size_t cas_node = (addr / granularity) % base_distribution<instance>::nodes;

	/* fetch the ownership value for the page from the local window */
	argo::backend::atomic::_load_local_owners_dir(&ownership,
			single_entry_size, rank, owners_dir_window_index+2);
	/* check if no info is found locally regarding the page */
	if (ownership == global_null) {
		/* load page info from the public cas_node window */
		std::size_t page_info[kPageInfoSize];
		argo::backend::atomic::_load_public_owners_dir(page_info, sizeof(std::size_t), kPageInfoSize, cas_node, owners_dir_window_index);
		/* check if any page info is found on the cas_node window */
		if (!is_all_equal_to(page_info, global_null)) {
			/* make sure that all the remote values are read correctly */
			if (rank != cas_node) {
				while (is_one_equal_to(page_info, global_null)) {
					argo::backend::atomic::_load_public_owners_dir(page_info, sizeof(std::size_t), kPageInfoSize, cas_node, owners_dir_window_index);
				}
				/* store page info in the local window */
				argo::backend::atomic::_store_local_owners_dir(page_info, kPageInfoSize, rank, owners_dir_window_index);
			}
		} else {
			if(do_first_touch) {
				/* try to claim ownership of that page */
				first_touch(addr);
			}
		}
	}
}

template<int instance>
void first_touch_distribution<instance>::first_touch(const std::size_t& addr) {
	/* variables for CAS */
	std::size_t offset, homenode, result;
	static const std::size_t rank = argo::backend::node_id();
	static const std::size_t global_null = base_distribution<instance>::total_size + 1;
	const std::size_t owners_dir_window_index = kPageInfoSize * (addr / granularity);
	/* decentralize CAS */
	const std::size_t cas_node = (addr / granularity) % base_distribution<instance>::nodes;

	/* check/try to acquire ownership of the page with CAS to process' cas_node index */
	argo::backend::atomic::_compare_exchange_owners_dir(&rank, &global_null, &result, sizeof(std::size_t), cas_node, owners_dir_window_index+2);

	/* this process was the first one to deposit its rank */
	if (result == global_null) {
		/* iterate through the nodes to find a valid offset for the page, starting from the currently running one */
		bool succeeded = false;
		std::size_t n, searched;
		for (n = rank, searched = 0; searched < static_cast<std::size_t>(base_distribution<instance>::nodes);
				n = (n + 1) % base_distribution<instance>::nodes, searched++) {
			/* load backing offset for the node from the offsets table */
			argo::backend::atomic::_load_local_offsets_tbl(&offset, rank, n);
			/* check if the backing store size of this node is sufficient */
			while (offset < base_distribution<instance>::size_per_node) {
				/* try to claim a valid offset */
				const std::size_t incr_offset = offset + granularity;
				argo::backend::atomic::_compare_exchange_offsets_tbl(&incr_offset, &offset, &result, sizeof(std::size_t), n, n);
				if (result == offset) {
					succeeded = true;
					homenode = n;
					break;
				}
				offset = result;
			}
			/* cache the offset returned from a remote CAS operation */
			if (n != rank) {
				argo::backend::atomic::_store_local_offsets_tbl(offset, rank, n);
			}
			/* succeeded, leave */
			if (succeeded) break;
		}

		/* this should never happen */
		if (!succeeded) {
			std::cerr << msg_first_touch_fail << std::endl;
			throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), msg_first_touch_fail);
		}

		/* store page info in the local window */
		std::size_t page_info[kPageInfoSize] = {homenode, offset, rank};
		argo::backend::atomic::_store_local_owners_dir(page_info, kPageInfoSize, rank, owners_dir_window_index);

		/* store page info in the remote public cas_node window */
		if (rank != cas_node) {
			argo::backend::atomic::_store_public_owners_dir(page_info, sizeof(std::size_t), kPageInfoSize, cas_node, owners_dir_window_index);
		}
	} else {
		/* load page info from the remote public cas_node window */
		if (rank != cas_node) {
			std::size_t page_info[kPageInfoSize];
			do {
				argo::backend::atomic::_load_public_owners_dir(page_info, sizeof(std::size_t), kPageInfoSize, cas_node, owners_dir_window_index);
			} while (is_one_equal_to(page_info, global_null));
			/* store page info in the local window */
			argo::backend::atomic::_store_local_owners_dir(page_info, kPageInfoSize, rank, owners_dir_window_index);
		}
	}
}

}  // namespace data_distribution
}  // namespace argo

#endif  // ARGODSM_SRC_DATA_DISTRIBUTION_FIRST_TOUCH_DISTRIBUTION_HPP_

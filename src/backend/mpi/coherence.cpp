/**
 * @file
 * @brief This file implements selective coherence mechanisms for ArgoDSM
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#include <shared_mutex>
#include <vector>

#include "../backend.hpp"
#include "swdsm.h"
#include "virtual_memory/virtual_memory.hpp"

namespace argo {
	namespace backend {
		void _selective_acquire(void *addr, std::size_t size) {
			// Skip selective acquire if the size of the region is 0
			if(size == 0) {
				return;
			}

			const std::size_t block_size = page_size*CACHELINE;
			const std::size_t start_address = reinterpret_cast<std::size_t>(argo::virtual_memory::start_address());
			const std::size_t page_misalignment = reinterpret_cast<std::size_t>(addr)%block_size;
			std::size_t argo_address =
				((reinterpret_cast<std::size_t>(addr)-start_address)/block_size)*block_size;
			const node_id_t node_id = argo::backend::node_id();
			const std::size_t node_id_bit = static_cast<std::size_t>(1) << node_id;

			// Lock relevant mutexes. Start statistics timekeeping
			double t1 = MPI_Wtime();
			std::shared_lock lock(sync_lock);

			// Iterate over all pages to selectively invalidate
			for(std::size_t page_address = argo_address;
					page_address < argo_address + page_misalignment + size;
					page_address += block_size) {
				const node_id_t homenode_id = peek_homenode(page_address);
				// This page should be skipped in the following cases
				// 1. The page is node local so no acquire is necessary
				// 2. The page has not yet been first-touched and trying
				// to perform an acquire would first-touch the page
				if(	homenode_id == node_id ||
					homenode_id == argo::data_distribution::invalid_node_id) {
					continue;
				}

				const std::size_t cache_index = getCacheIndex(page_address);
				const std::size_t classification_index = get_classification_index(page_address);
				cache_locks[cache_index].lock();

				// If the page is dirty, downgrade it
				if(cacheControl[cache_index].dirty == DIRTY) {
					mprotect(reinterpret_cast<char*>(start_address) + page_address, block_size, PROT_READ);
					for(int i = 0; i < CACHELINE; i++) {
						storepageDIFF(cache_index+i, page_address+page_size*i);
					}
					argo_write_buffer->erase(cache_index);
					cacheControl[cache_index].dirty = CLEAN;
				}

				std::size_t win_index = get_sharer_win_index(classification_index);
				// Optimization to keep pages in cache if they do not
				// need to be invalidated.
				mpi_lock_sharer[win_index][node_id].lock(MPI_LOCK_SHARED, node_id, sharer_windows[win_index][node_id]);
				if(
						// node is single writer
						(globalSharers[classification_index+1] == node_id_bit)
						||
						// No writer and assert that the node is a sharer
						((globalSharers[classification_index+1] == 0) &&
						 ((globalSharers[classification_index] & node_id_bit) == node_id_bit))
				  ) {
					mpi_lock_sharer[win_index][node_id].unlock(node_id, sharer_windows[win_index][node_id]);
					touchedcache[cache_index] = 1;
					// nothing - we keep the pages, SD is done in flushWB
				} else { // multiple writer or SO, invalidate the page
					mpi_lock_sharer[win_index][node_id].unlock(node_id, sharer_windows[win_index][node_id]);
					cacheControl[cache_index].dirty = CLEAN;
					cacheControl[cache_index].state = INVALID;
					touchedcache[cache_index] = 0;
					mprotect(reinterpret_cast<char*>(start_address) + page_address, block_size, PROT_NONE);
				}
				cache_locks[cache_index].unlock();
			}
			double t2 = MPI_Wtime();

			// Poke the MPI system to force progress
			int flag;
			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, workcomm, &flag, MPI_STATUS_IGNORE);

			std::lock_guard<std::mutex> ssi_time_lock(stats.ssi_time_mutex);
			stats.ssi_time += t2-t1;
		}

		void _selective_release(void *addr, std::size_t size) {
			// Skip selective release if the size of the region is 0
			if(size == 0) {
				return;
			}

			const std::size_t block_size = page_size*CACHELINE;
			const std::size_t start_address = reinterpret_cast<std::size_t>(argo::virtual_memory::start_address());
			const std::size_t page_misalignment = reinterpret_cast<std::size_t>(addr)%block_size;
			std::size_t argo_address =
				((reinterpret_cast<std::size_t>(addr)-start_address)/block_size)*block_size;
			const node_id_t node_id = argo::backend::node_id();

			// Lock relevant mutexes. Start statistics timekeeping
			double t1 = MPI_Wtime();
			std::shared_lock lock(sync_lock);

			// Iterate over all pages to selectively downgrade
			for(std::size_t page_address = argo_address;
					page_address < argo_address + page_misalignment + size;
					page_address += block_size) {
				const node_id_t homenode_id = peek_homenode(page_address);
				// selective_release should be skipped in the following cases
				// 1. The page is node local so no release is necessary
				// 2. The page has not yet been first-touched and trying
				// to perform a release would first-touch the page
				if(	homenode_id == node_id ||
					homenode_id == argo::data_distribution::invalid_node_id) {
					continue;
				}

				const std::size_t cache_index = getCacheIndex(page_address);
				cache_locks[cache_index].lock();

				// If the page is dirty, downgrade it
				if(cacheControl[cache_index].dirty == DIRTY) {
					mprotect(reinterpret_cast<char*>(start_address) + page_address, block_size, PROT_READ);
					for(int i = 0; i <CACHELINE; i++) {
						storepageDIFF(cache_index+i, page_address+page_size*i);
					}
					argo_write_buffer->erase(cache_index);
					cacheControl[cache_index].dirty = CLEAN;
				}
				cache_locks[cache_index].unlock();
			}
			double t2 = MPI_Wtime();

			// Poke the MPI system to force progress
			int flag;
			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, workcomm, &flag, MPI_STATUS_IGNORE);

			std::lock_guard<std::mutex> ssd_time_lock(stats.ssd_time_mutex);
			stats.ssd_time += t2-t1;
		}
	} //namespace backend
} //namespace argo

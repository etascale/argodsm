/**
 * @file
 * @brief This file provides an interface for the ArgoDSM write buffer.
 * @copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
 */

#ifndef argo_write_buffer_hpp
#define argo_write_buffer_hpp argo_write_buffer_hpp

#include <deque>
#include <iterator>
#include <algorithm>
#include <mutex>
#include <mpi.h>

#include "backend/backend.hpp"
#include "env/env.hpp"
#include "virtual_memory/virtual_memory.hpp"
#include "swdsm.h"

/**
 * @brief		Argo statistics struct
 * @deprecated 	This should be replaced with an API call
 */
extern argo_statistics stats;

/**
 * @brief		Argo cache data structure
 * @deprecated 	prototype implementation, should be replaced with API calls
 */
extern control_data* cacheControl;

/**
 * @brief		MPI Window for the global ArgoDSM memory space
 * @deprecated 	Prototype implementation, this should not be accessed directly
 */
extern MPI_Win* globalDataWindow;

/**
 * @brief		Tracking of which MPI RDMA windows are open
 * @deprecated 	Prototype implementation, this should not be accessed directly
 */
extern char* barwindowsused;

/** @brief Block size based on backend definition */
const std::size_t block_size = page_size*CACHELINE;

/**
 * @brief	A write buffer in FIFO style with the capability to erase any
 * element from the buffer while preserving ordering.
 * @tparam	T the type of the write buffer
 */
template<typename T>
class write_buffer
{
	private:
		/** @brief This container holds cache indexes that should be written back */
		std::deque<T> _buffer;

		/** @brief The maximum size of the write buffer */
		std::size_t _max_size;

		/**
		 * @brief The number of elements to write back once attempting to
		 * add an element to an already full write buffer.
		 */
		std::size_t _write_back_size;

		/** @brief	Mutex to protect the write buffer from simultaneous accesses */
		mutable std::mutex _buffer_mutex;

		/**
		 * @brief	Check if the write buffer is empty
		 * @return	True if empty, else False
		 */
		bool empty() {
			return _buffer.empty();
		}

		/**
		 * @brief	Get the size of the buffer
		 * @return	The size of the buffer
		 */
		size_t size() {
			return _buffer.size();
		}

		/**
		 * @brief	Get the buffer element at index i
		 * @param	i The requested buffer index
		 * @return	The element at index i of type T
		 */
		T at(std::size_t i){
			return _buffer.at(i);
		}

		/**
		 * @brief	Check if val exists in the buffer
		 * @param	val The value to check for
		 * @return	True if val exists in the buffer, else False
		 */
		bool has(T val) {
			typename std::deque<T>::iterator it = std::find(_buffer.begin(),
					_buffer.end(), val);
			return (it != _buffer.end());
		}

		/**
		 * @brief	Constructs a new element and emplaces it at the back of the buffer
		 * @param	args Properties of the new element to emplace back
		 */
		template<typename... Args>
			void emplace_back( Args&&... args) {
				_buffer.emplace_back(std::forward<Args>(args)...);
			}

		/**
		 * @brief	Removes the front element from the buffer and returns it
		 * @return	The element that was popped from the buffer
		 */
		T pop() {
			auto elem = std::move(_buffer.front());
			_buffer.pop_front();
			return elem;
		}

		/**
		 * @brief	Sorts all elements by home node id in ascending order
		 */
		void sort() {
			std::sort(_buffer.begin(), _buffer.end(),
					[](const T& l, const T& r) {
				return get_homenode(cacheControl[l].tag) < get_homenode(cacheControl[r].tag);
			});
		}

		/**
		 * @brief	Sorts the first _write_back_size elements by home node id in ascending order
		 */
		void sort_first() {
			assert(_buffer.size() >= _write_back_size);
			std::sort(_buffer.begin(), _buffer.begin()+_write_back_size,
					[](const T& l, const T& r) {
				return get_homenode(cacheControl[l].tag) < get_homenode(cacheControl[r].tag);
			});
		}

		/**
		 * @brief	Flushes first _write_back_size elements of the  ArgoDSM 
		 * 			write buffer to memory
		 * @pre		Require ibsem to be taken until parallel MPI
		 * @pre		Require write_buffer_mutex to be held
		 */
		void flush_partial() {
			// Sort the first _write_back_size elements
			sort_first();

			// For each element, handle the corresponding ArgoDSM page
			for(std::size_t i = 0; i < _write_back_size; i++) {
				// The code below should be replaced with a cache API
				// call to write back a cached page
				std::size_t cache_index = pop();
				std::uintptr_t page_address = cacheControl[cache_index].tag;
				void* page_ptr = static_cast<char*>(
						argo::virtual_memory::start_address()) + page_address;

				// Write back the page
				mprotect(page_ptr, block_size, PROT_READ);
				cacheControl[cache_index].dirty=CLEAN;
				for(int i=0; i < CACHELINE; i++){
					storepageDIFF(cache_index+i,page_size*i+page_address);
				}
			}

			// Close any windows used to write back data
			// This should be replaced with an API call
			for(int i = 0; i < argo::backend::number_of_nodes(); i++){
				if(barwindowsused[i] == 1){
					MPI_Win_unlock(i, globalDataWindow[i]);
					barwindowsused[i] = 0;
				}
			}
		}

	public:
		/**
		 * @brief	Constructor
		 */
		write_buffer()	:
			_max_size(argo::env::write_buffer_size()/CACHELINE),
			_write_back_size(argo::env::write_buffer_write_back_size()/CACHELINE) { }

		/**
		 * @brief	Copy constructor
		 * @param	other The write_buffer object to copy from
		 */
		write_buffer(const write_buffer & other) {
			// Ensure protection of data
			std::lock_guard<std::mutex> lock_other(other._buffer_mutex);
			// Copy data
			_buffer = other._buffer;
			_max_size = other._max_size;
			_write_back_size = other._write_back_size;
		}

		/**
		 * @brief	Copy assignment operator
		 * @param	other the write_buffer object to copy assign from
		 * @return	reference to the created copy
		 */
		write_buffer& operator=(const write_buffer & other) {
			if(&other != this) {
				// Ensure protection of data
				std::unique_lock<std::mutex> lock_this(_buffer_mutex, std::defer_lock);
				std::unique_lock<std::mutex> lock_other(other._buffer_mutex, std::defer_lock);
				std::lock(lock_this, lock_other);
				// Copy data
				_buffer = other._buffer;
				_max_size = other._max_size;
				_write_back_size = other._write_back_size;
			}
			return *this;
		}

		/**
		 * @brief	If val exists in buffer, delete it. Else, do nothing.
		 * @param	val The value of type T to erase
		 */
		void erase(T val) {
			std::lock_guard<std::mutex> lock(_buffer_mutex);

			typename std::deque<T>::iterator it = std::find(_buffer.begin(),
					_buffer.end(), val);
			if(it != _buffer.end()){
				_buffer.erase(it);
			}
		}

		/**
		 * @brief	Flushes the ArgoDSM write buffer to memory
		 * @pre		Require ibsem to be taken until parallel MPI
		 */
		void flush() {
			double t_start = MPI_Wtime();
			std::lock_guard<std::mutex> lock(_buffer_mutex);

			// Sort the write buffer if needed
			if(!empty()) {
				sort();
			}

			// For each element, handle the corresponding ArgoDSM page
			while(!empty()) {
				// The code below should be replaced with a cache API
				// call to write back a cached page
				std::size_t cache_index = pop();
				std::uintptr_t page_address = cacheControl[cache_index].tag;
				void* page_ptr = static_cast<char*>(
						argo::virtual_memory::start_address()) + page_address;

				// Write back the page
				mprotect(page_ptr, block_size, PROT_READ);
				cacheControl[cache_index].dirty=CLEAN;
				for(int i=0; i < CACHELINE; i++){
					storepageDIFF(cache_index+i,page_size*i+page_address);
				}
			}

			// Close any windows used to write back data
			// This should be replaced with an API call
			for(int i = 0; i < argo::backend::number_of_nodes(); i++){
				if(barwindowsused[i] == 1){
					MPI_Win_unlock(i, globalDataWindow[i]);
					barwindowsused[i] = 0;
				}
			}

			// Update timer statistics
			double t_stop = MPI_Wtime();
			stats.flushtime = t_stop-t_start;
		}

		/**
		 * @brief	Adds a new element to the write buffer
		 * @param	val The value of type T to add to the buffer
		 * @pre		Require ibsem to be taken until parallel MPI
		 */
		void add(T val) {
			std::lock_guard<std::mutex> lock(_buffer_mutex);

			// If already present in the buffer, do nothing
			if(has(val)){
				return;
			}

			// If the buffer is full, write back _write_back_size indices
			if(size() >= _max_size){
				double t_start = MPI_Wtime();
				flush_partial();
				double t_end = MPI_Wtime();
				stats.writebacks+=CACHELINE;
				stats.writebacktime+=t_end-t_start;
			}

			// Add val to the back of the buffer
			emplace_back(val);
		}

}; //class

#endif /* argo_write_buffer_hpp */

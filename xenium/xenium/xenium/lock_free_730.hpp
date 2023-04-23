//
// Copyright (c) 2018-2020 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_LOCK_FREE_730_HPP
#define XENIUM_LOCK_FREE_730_HPP

#include <xenium/acquire_guard.hpp>
#include <xenium/backoff.hpp>
#include <xenium/marked_ptr.hpp>
#include <xenium/parameter.hpp>
#include <xenium/policy.hpp>
#include <atomic>

#ifdef _MSC_VER
  #pragma warning(push)
  #pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

//0 = ins
//1 = rem
//2 = DAT
//3 = INV
#define INSERT 1
#define REMOVE 2
#define DATA 3
#define DEAD 4

namespace xenium {
/**
 * @brief An unbounded generic lock-free multi-producer/multi-consumer FIFO queue.
 *
 * This is an implementation of the lock-free MPMC queue proposed by Michael and Scott
 * \[[MS96](index.html#ref-michael-1996)\].
 * It is fully generic and can handle any type `T` that is copyable or movable.
 *
 * Supported policies:
 *  * `xenium::policy::reclaimer`<br>
 *    Defines the reclamation scheme to be used for internal nodes. (**required**)
 *  * `xenium::policy::backoff`<br>
 *    Defines the backoff strategy. (*optional*; defaults to `xenium::no_backoff`)
 *
 * @tparam T type of the stored elements.
 * @tparam Policies list of policies to customize the behaviour
 */
template <class T, class... Policies>
class lock_free_730 {
public:
  using value_type = T;
  using reclaimer = parameter::type_param_t<policy::reclaimer, parameter::nil, Policies...>;
  using backoff = parameter::type_param_t<policy::backoff, no_backoff, Policies...>;

  template <class... NewPolicies>
  using with = lock_free_730<T, NewPolicies..., Policies...>;

  static_assert(parameter::is_set<reclaimer>::value, "reclaimer policy must be specified");

  lock_free_730();
  ~lock_free_730();

  /**
   * @brief Pushes the given value to the queue.
   *
   * This operation always allocates a new node.
   * Progress guarantees: lock-free (always performs a memory allocation)
   *
   * @param value
   */
	void enlist(node& value);
	bool helpInsert(node& home, T key);
	bool helpRemove(node& home, T key);
	bool contains(T key);
	bool insert(T key);
	bool remove(T key);

private:
  struct node;

  using concurrent_ptr = typename reclaimer::template concurrent_ptr<node, 0>;
  using marked_ptr = typename concurrent_ptr::marked_ptr;
  using guard_ptr = typename concurrent_ptr::guard_ptr;

  struct node : reclaimer::template enable_concurrent_ptr<node> {
    node() : _value(){};
    explicit node(T&& v, unsigned char _s) : _value(std::move(v)), _state(_s) {}

    T _value;
    concurrent_ptr _next;
		concurrent_ptr _prev;
		size_t tid;
		atomic_uchar _state;
		//0 = ins
		//1 = rem
		//2 = DAT
		//3 = INV
  };

  alignas(64) concurrent_ptr _head;
  //alignas(64) concurrent_ptr _tail;
};

template <class T, class... Policies>
lock_free_730<T, Policies...>::lock_free_730() {
  //auto n = new node();
  _head.store(nullptr, std::memory_order_relaxed);
  //_tail.store(n, std::memory_order_relaxed);
}

template <class T, class... Policies>
lock_free_730<T, Policies...>::~lock_free_730() {
  // (1) - this acquire-load synchronizes-with the release-CAS (11)
  auto n = _head.load(std::memory_order_acquire);
  while (n) {
    // (2) - this acquire-load synchronizes-with the release-CAS (6)
    auto next = n->_next.load(std::memory_order_acquire);
    delete n.get();
    n = next;
  }
}

template <class T, class... Policies>
void lock_free_730<T, Policies...>::enlist(node* nn) {
	guard_ptr old;
	marked_ptr n(nn);
	marked_ptr t(old.get());
	do {
		old.acquire(_head, std::memory_order_acquire);
		t = marked_ptr(old.get());
		n->_next.store(t, std::memory_order_relaxed);
	} while (!(_head.compare_exchange_weak(t, n, std::memory_order_release, std::memory_order_relaxed)))
}

template <class T, class... Policies>
bool lock_free_730<T, Policies...>::helpInsert(node* n, T key) {
	node* pred = n;
	auto curr = n->_next.load(std::memory_order_acquire);
	
	while (curr) {
		unsigned char s = curr->_state.load(std::memory_order_acquire);
		if (s == DEAD) {
			auto succ = curr->_next.load(std::memory_order_acquire);
			pred->_next->store(succ, std::memory_order_relaxed);
			curr = succ;
		} else if (curr->_value != key) {
			pred = curr.get();
			curr = curr->_next.load(std::memory_order_acquire);
		} else {
			return (s == REMOVE);
		}
	}
	return true;
}

template <class T, class... Policies>
bool lock_free_730<T, Policies...>::helpRemove(node* n, T key) {
	node* pred = n;
	auto curr = n->_next.load(std::memory_order_acquire);
	
	while (curr) {
		unsigned char s = curr->_state.load(std::memory_order_acquire);
		if (s == DEAD) {
			auto succ = curr->_next.load(std::memory_order_acquire);
			pred->_next->store(succ, std::memory_order_relaxed);
			curr = succ;
		} else if (curr->_value != key) {
			pred = curr.get();
			curr = curr->_next.load(std::memory_order_acquire);
		} else if (s == DATA) {
			curr->_state = DEAD;
			return true;
		} else if (s == REMOVE) {
			return false;
		} else if (s == INSERT) {
			
			if (curr->_state.compare_exchange_strong(INSERT, REMOVE, std::memory_order_release, std::memory_order_relaxed)) {
				return true;
			}
		}
	}
	return false;
}

template <class T, class... Policies>
bool lock_free_730<T, Policies...>::contains(T key) {
	
	auto curr = _head.load(std::memory_order_acquire);
	
	while (curr) {
		if (curr->_value == key) {
			unsigned char s = curr->_state.load(std::memory_order_acquire);
			if (s != DEAD) {
				return (s != REMOVE);
			}
		}
		curr = curr->_next.load(std::memory_order_acquire);
	}
	return false;
}

template <class T, class... Policies>
bool lock_free_730<T, Policies...>::insert(T key) {
	node* n = new node(key, INSERT);
	enlist(n);
	bool b = helpInsert(n, key);
	unsigned char s = b ? DATA : DEAD;
	if (!(n->_state.compare_exchange_strong(INSERT, s, std::memory_order_release, std::memory_order_relaxed))) {
		helpRemove(n, key);
		n->_state.store(DEAD, std::memory_order_relaxed);
	}
	return b;
}

template <class T, class... Policies>
bool lock_free_730<T, Policies...>::remove(T key) {
	node* n = new node(key, REMOVE);
	enlist(n);
	bool b = helpRemove(n, key);
	n->_state.store(DEAD, std::memory_order_relaxed);
	return b;
}

} // namespace xenium

#ifdef _MSC_VER
  #pragma warning(pop)
#endif

#endif

#include <xenium/lock_free_730.hpp>
#include <xenium/reclamation/generic_epoch_based.hpp>
#include <xenium/reclamation/hazard_eras.hpp>
#include <xenium/reclamation/hazard_pointer.hpp>
#include <xenium/reclamation/lock_free_ref_count.hpp>
#include <xenium/reclamation/quiescent_state_based.hpp>
#include <xenium/reclamation/stamp_it.hpp>

#include <gtest/gtest.h>

#include <thread>
#include <vector>
#include <set>

/*
./gtest --gtest_filter=*LockFree730*
./gtest --gtest_filter=*MichaelScottQueue*
*/
namespace {

template <typename Reclaimer>
struct LockFree730 : testing::Test {};

using Reclaimers =
  ::testing::Types<xenium::reclamation::lock_free_ref_count<>,
                   xenium::reclamation::hazard_pointer<>::with<
                     xenium::policy::allocation_strategy<xenium::reclamation::hp_allocation::static_strategy<2>>>,
                   xenium::reclamation::hazard_eras<>::with<
                     xenium::policy::allocation_strategy<xenium::reclamation::he_allocation::static_strategy<2>>>,
                   xenium::reclamation::quiescent_state_based,
                   xenium::reclamation::stamp_it,
                   xenium::reclamation::epoch_based<>::with<xenium::policy::scan_frequency<10>>,
                   xenium::reclamation::new_epoch_based<>::with<xenium::policy::scan_frequency<10>>,
                   xenium::reclamation::debra<>::with<xenium::policy::scan_frequency<10>>>;
TYPED_TEST_SUITE(LockFree730, Reclaimers);

TYPED_TEST(LockFree730, push_try_pop_returns_pushed_element) {
  xenium::lock_free_730<int, xenium::policy::reclaimer<TypeParam>> queue;
  queue.insert(42);
  //int elem = 0;
  ASSERT_TRUE(queue.remove(42));
  //EXPECT_EQ(42, elem);
}

TYPED_TEST(LockFree730, insert_100_random_numbers_and_delete_in_random_order) {
  xenium::lock_free_730<int, xenium::policy::reclaimer<TypeParam>> queue;
	std::vector<int> nums(100);
	std::set<int> num2;
	
	for (int i = 0; i < 100; i++) {
		int x = rand() % 1000;
		while (num2.find(x) != end(num2)) {
			x = rand() % 1000;
		}
		nums[i] = x;
		num2.insert(x);
		EXPECT_TRUE(queue.insert(x));
	}
	
	while (nums.size()) {
		int pos = rand() % nums.size();
		EXPECT_TRUE(queue.remove(nums[pos]));
		std::swap(nums[pos], nums[nums.size() - 1]);
		nums.pop_back();
	}
}

TYPED_TEST(LockFree730, push_two_items_pop_them_in_FIFO_order) {
  xenium::lock_free_730<int, xenium::policy::reclaimer<TypeParam>> queue;
  EXPECT_TRUE(queue.insert(42));
  EXPECT_TRUE(queue.insert(43));
  //int elem1 = 0;
  //int elem2 = 0;
  EXPECT_TRUE(queue.remove(42));
  EXPECT_TRUE(queue.remove(43));
  //EXPECT_EQ(42, elem1);
  //EXPECT_EQ(43, elem2);
}
/*
TYPED_TEST(LockFree730, supports_move_only_types) {
  xenium::lock_free_730<std::unique_ptr<int>, xenium::policy::reclaimer<TypeParam>> queue;
  queue.insert(std::make_unique<int>(42));

  std::unique_ptr<int> elem;
  ASSERT_TRUE(queue.remove(elem));
  ASSERT_NE(nullptr, elem);
  EXPECT_EQ(42, *elem);
}
*/

TYPED_TEST(LockFree730, parallel_usage) {
  using Reclaimer = TypeParam;
  xenium::lock_free_730<int, xenium::policy::reclaimer<Reclaimer>> queue;

  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.push_back(std::thread([i, &queue] {
#ifdef DEBUG
      const int MaxIterations = 1000;
#else
      const int MaxIterations = 10000;
#endif
      for (int j = 0; j < MaxIterations; ++j) {
        [[maybe_unused]] typename Reclaimer::region_guard guard{};
        queue.insert(i);
        //int v;
        EXPECT_TRUE(queue.remove(i));
      }
    }));
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

} // namespace
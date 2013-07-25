/*
 * Copyright 2013 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: pulkitg@google.com (Pulkit Goyal)

// Unit test for CachePropertyStore.

#include "net/instaweb/util/public/cache_property_store.h"

#include <cstddef>

#include "net/instaweb/util/property_cache.pb.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mock_property_page.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/thread_system.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"

namespace net_instaweb {

namespace {

const size_t kMaxCacheSize = 100;
const char kCohortName1[] = "cohort1";
const char kCohortName2[] = "cohort2";
const char kUrl[] = "www.test.com/sample.html";
const char kParsableContent[] =
    "value { name: 'prop1' value: 'value1' }";
const char kNonParsableContent[] = "random";
const char kOptionsSignatureHash[] = "hash";

}  // namespace

class CachePropertyStoreTest : public testing::Test {
 public:
  CachePropertyStoreTest()
     : lru_cache_(kMaxCacheSize),
       timer_(MockTimer::kApr_5_2010_ms),
       thread_system_(Platform::CreateThreadSystem()),
       cache_property_store_(
           "test/", &lru_cache_, &timer_, &stats_, thread_system_.get()),
       property_cache_(&cache_property_store_,
                       &timer_,
                       &stats_,
                       thread_system_.get()),
       num_callback_with_false_called_(0),
       num_callback_with_true_called_(0) {
    PropertyCache::InitCohortStats(kCohortName1, &stats_);
    cohort_ = property_cache_.AddCohort(kCohortName1);
    cache_property_store_.AddCohort(kCohortName1);
    cohort_list_.push_back(cohort_);
  }

  void SetUp() {
    page_.reset(
        new MockPropertyPage(
            thread_system_.get(),
            &property_cache_,
            kUrl,
            kOptionsSignatureHash,
            UserAgentMatcher::kDesktop));
    property_cache_.Read(page_.get());
  }

  void ExpectFalse(bool result) {
    EXPECT_FALSE(result);
    ++num_callback_with_false_called_;
  }

  void ExpectTrue(bool result) {
    EXPECT_TRUE(result);
    ++num_callback_with_true_called_;
  }

  void ExpectNumCallbacksCalled(int expected_num_callback_with_false_called,
                                int expected_num_callback_with_true_called) {
    EXPECT_EQ(expected_num_callback_with_false_called,
              num_callback_with_false_called_);
    EXPECT_EQ(expected_num_callback_with_true_called,
              num_callback_with_true_called_);
  }

  void ExpectCacheStats(LRUCache* lru_cache,
                        int expected_num_cache_hits,
                        int expected_num_cache_misses,
                        int expected_num_cache_inserts) {
    EXPECT_EQ(expected_num_cache_hits, lru_cache->num_hits());
    EXPECT_EQ(expected_num_cache_misses, lru_cache->num_misses());
    EXPECT_EQ(expected_num_cache_inserts, lru_cache->num_inserts());
  }

 protected:
  LRUCache lru_cache_;
  MockTimer timer_;
  SimpleStats stats_;
  scoped_ptr<ThreadSystem> thread_system_;
  CachePropertyStore cache_property_store_;
  PropertyCache property_cache_;
  const PropertyCache::Cohort* cohort_;
  PropertyCache::CohortVector cohort_list_;
  scoped_ptr<MockPropertyPage> page_;
  int num_callback_with_false_called_;
  int num_callback_with_true_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CachePropertyStoreTest);
};

TEST_F(CachePropertyStoreTest, TestNoResultAvailable) {
  cache_property_store_.Get(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_list_,
      page_.get(),
      NewCallback(this, &CachePropertyStoreTest::ExpectFalse));
  EXPECT_EQ(CacheInterface::kNotFound, page_->GetCacheState(cohort_));
  ExpectNumCallbacksCalled(1,  /* Num Callback with false */
                           0  /* Num Callback with true */);
}

TEST_F(CachePropertyStoreTest, TestResultAvailable) {
  PropertyCacheValues values;
  values.ParseFromString(kParsableContent);
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_,
      &values,
      NULL);
  cache_property_store_.Get(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_list_,
      page_.get(),
      NewCallback(this, &CachePropertyStoreTest::ExpectTrue));
  EXPECT_EQ(CacheInterface::kAvailable, page_->GetCacheState(cohort_));
  ExpectNumCallbacksCalled(0,  /* Num Callback with false */
                           1  /* Num Callback with true */);
}

TEST_F(CachePropertyStoreTest, TestResultAvailableButNonParsable) {
  SharedString put_buffer(kNonParsableContent);
  lru_cache_.Put(cache_property_store_.CacheKey(kUrl,
                                                kOptionsSignatureHash,
                                                UserAgentMatcher::kDesktop,
                                                cohort_),
                 &put_buffer);
  cache_property_store_.Get(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_list_,
      page_.get(),
      NewCallback(this, &CachePropertyStoreTest::ExpectFalse));
  EXPECT_EQ(CacheInterface::kAvailable, page_->GetCacheState(cohort_));
  ExpectNumCallbacksCalled(1,  /* Num Callback with false */
                           0  /* Num Callback with true */);
}

TEST_F(CachePropertyStoreTest, TestMultipleCohorts) {
  PropertyCache::InitCohortStats(kCohortName2, &stats_);
  const PropertyCache::Cohort* cohort2 =
      property_cache_.AddCohort(kCohortName2);
  cache_property_store_.AddCohort(kCohortName2);
  MockPropertyPage page(thread_system_.get(),
                        &property_cache_,
                        kUrl,
                        kOptionsSignatureHash,
                        UserAgentMatcher::kDesktop);
  property_cache_.Read(&page);
  PropertyCacheValues values;
  values.ParseFromString(kParsableContent);
  cohort_list_.push_back(cohort2);
  lru_cache_.ClearStats();
  cache_property_store_.Get(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_list_,
      &page,
      NewCallback(this, &CachePropertyStoreTest::ExpectFalse));
  ExpectCacheStats(&lru_cache_,
                   0,  /* Cache hit */
                   2,  /* Cache miss */
                   0  /* Cache inserts */);

  lru_cache_.ClearStats();
  // Insert the value for cohort1.
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_,
      &values,
      NULL);
  cache_property_store_.Get(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_list_,
      &page,
      NewCallback(this, &CachePropertyStoreTest::ExpectTrue));
  ExpectCacheStats(&lru_cache_,
                   1,  /* Cache hit */
                   1,  /* Cache miss */
                   1  /* Cache inserts */);

  lru_cache_.ClearStats();
  // Insert the value for cohort2.
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort2,
      &values,
      NULL);
  cache_property_store_.Get(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_list_,
      &page,
      NewCallback(this, &CachePropertyStoreTest::ExpectTrue));
  ExpectCacheStats(&lru_cache_,
                   2,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */);
  ExpectNumCallbacksCalled(1,  /* Num Callback with false */
                           2  /* Num Callback with true */);
}

TEST_F(CachePropertyStoreTest, TestMultipleCacheBackends) {
  // Create a second cache implementation.
  LRUCache second_cache(kMaxCacheSize);
  PropertyCache::InitCohortStats(kCohortName2, &stats_);
  const PropertyCache::Cohort* cohort2 =
      property_cache_.AddCohort(kCohortName2);
  cache_property_store_.AddCohortWithCache(kCohortName2, &second_cache);
  MockPropertyPage page(
      thread_system_.get(),
      &property_cache_,
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop);
  property_cache_.Read(&page);
  PropertyCacheValues values;
  values.ParseFromString(kParsableContent);
  lru_cache_.ClearStats();
  second_cache.ClearStats();
  // Insert the value for cohort1.
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_,
      &values,
      NULL);
  // Insert the value for cohort2.
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort2,
      &values,
      NULL);
  // Get the value for cohort1 and cohort2.
  cohort_list_.push_back(cohort2);
  cache_property_store_.Get(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_list_,
      &page,
      NewCallback(this, &CachePropertyStoreTest::ExpectTrue));
  EXPECT_EQ(CacheInterface::kAvailable, page.GetCacheState(cohort_));
  EXPECT_EQ(CacheInterface::kAvailable, page.GetCacheState(cohort2));
  ExpectCacheStats(&lru_cache_,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */);
  ExpectCacheStats(&second_cache,
                   1,  /* Cache hit */
                   0,  /* Cache miss */
                   1  /* Cache inserts */);
  ExpectNumCallbacksCalled(0,  /* Num Callback with false */
                           1  /* Num Callback with true */);
}

TEST_F(CachePropertyStoreTest, TestPropertyCacheKeyMethod) {
  GoogleString cache_key = cache_property_store_.CacheKey(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_);
  GoogleString expected = StrCat(
      "test/",
      kUrl, "_",
      kOptionsSignatureHash,
      UserAgentMatcher::DeviceTypeSuffix(UserAgentMatcher::kDesktop), "@",
      cohort_->name());
  EXPECT_EQ(expected, cache_key);
}

TEST_F(CachePropertyStoreTest, TestPutHandlesNonNullCallback) {
  PropertyCacheValues values;
  values.ParseFromString(kParsableContent);
  cache_property_store_.Put(
      kUrl,
      kOptionsSignatureHash,
      UserAgentMatcher::kDesktop,
      cohort_,
      &values,
      NewCallback(this, &CachePropertyStoreTest::ExpectTrue));
  ExpectNumCallbacksCalled(0,  /* Num Callback with false */
                           1  /* Num Callback with true */);
}

}  // namespace net_instaweb
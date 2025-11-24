/*
 * Copyright (C) 2025 The Android Open Source Project
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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_UNIT_TEST_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_UNIT_TEST_H_

#include <chre.h>
#include <thread>  // NOLINT

#include <gtest/gtest.h>
#include "absl/log/log.h"
#include "absl/synchronization/blocking_counter.h"
#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/platform/linux/thread_context.h"
#include "chre/platform/static_nanoapp_init.h"
#include "chre/util/system/event_callbacks.h"
#include "chre/util/unique_ptr.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_detector.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chrex_api_fake_detector.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chrex_api_fake_provider.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/audio_helpers.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/event_helpers.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/gnss_helpers.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/re_helpers.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/sensor_helpers.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/wifi_helpers.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/wwan_helpers.h"

namespace lbs {
namespace contexthub {

// Initializes nanoapp info and sets up a UniquePtr ready to be loaded
// into the CHRE system.
chre::UniquePtr<chre::Nanoapp> InitializeNanoapp(
    decltype(nanoappStart) *start, decltype(nanoappHandleEvent) *handle_event,
    decltype(nanoappEnd) *end);

// Test fixture that assists in managing the CHRE life cycle.
class ChreUnitTest : public ::testing::Test {
 public:
  // The event_type for actually running the NanoappTest.
  static constexpr int kStartNanoappTestEventType = 2000;

 protected:
  // Used to call helper functions that configure the CHRE Fake Api.
  virtual void ConfigureApi() {}

  // Starts the runtime.
  void StartChre();

  // Load the nanoapp into the system.
  void LoadNanoapp(chre::UniquePtr<chre::Nanoapp> &&nanoapp);

  // Terminates the runtime and frees any allocated memory.
  void ShutdownChre();

  // Used to dectect if a CHRE API call is done.
  // Usage: EXPECT_CALL(*chre_api_fake_detector_, chreGetTime).times(1)
  ChreApiDetector *chre_api_fake_detector_;

  // Used to dectect if a CHREX API call is done.
  ChrexApiDetector *chrex_api_fake_detector_;
};

// The name for the to-be-generated CHRE test subclass.
#define NANOAPP_TEST_CLASS_NAME_(test_fixture, test_name) \
  test_fixture##test_name

// The name for the to-be-generated CHRE test subclass's initializer.
#define NANOAPP_TEST_CLASS_INITIALIZER_NAME_(test_fixture, test_name) \
  test_fixture##_##test_name##_##init

// Allows running a nanoapp test without a test fixture
#define TEST_NANOAPP(test_case_name, test_name) \
  NANOAPP_TEST_(test_case_name, lbs::contexthub::ChreUnitTest, test_name)

// Uses a test fixture as the base class for the nanoapp test.
#define TEST_NANOAPP_F(test_fixture, test_name) \
  NANOAPP_TEST_(test_fixture, test_fixture, test_name)

// Generates test-specific functions that control the flow of the nanoapp
// execution. handle_event is the main function that actually executes the test
// (contained in NanoappTest), and nanoapp_run should be called on a separate
// thread since run() blocks.
//
// nanoapp_start/end/handle_event are the only functions that are run with a
// valid context, which is why we run NanoappTest in them.
#define NANOAPP_TEST_FUNCTIONS_(test_case_name, test_name)                    \
                                                                              \
  bool nanoapp_start##test_name() {                                           \
    return true;                                                              \
  }                                                                           \
  void nanoapp_end##test_name() {}                                            \
  void nanoapp_handle_event##test_name(uint32_t /* sender_instance_id */,     \
                                       uint16_t evt_type,                     \
                                       const void * /* evt_data */) {         \
    if (evt_type !=                                                           \
        lbs::contexthub::ChreUnitTest::kStartNanoappTestEventType) {          \
      LOG(ERROR)                                                              \
          << "An event with type " << evt_type                                \
          << " has been triggered. Please check that this event is required." \
          << std::endl;                                                       \
      return;                                                                 \
    }                                                                         \
    test_name##init_counter.DecrementCount();                                 \
    test_name##start_test_counter.Wait();                                     \
    test_case_name##_##test_name##_TestHook->NanoappTest();                   \
    test_name##end_test_counter.DecrementCount();                             \
  }                                                                           \
  void nanoapp_run##test_name() {                                             \
    chre::registerThreadContext(                                              \
        &chre::EventLoopManagerSingleton::get()->getEventLoop());             \
    chre::EventLoopManagerSingleton::get()->getEventLoop().postEventOrDie(    \
        lbs::contexthub::ChreUnitTest::kStartNanoappTestEventType, nullptr,   \
        chre::freeEventDataCallback, 1);                                      \
    chre::EventLoopManagerSingleton::get()->getEventLoop().run();             \
    test_name##deinit_counter.DecrementCount();                               \
  }

// Generates the function definitions for the initializer constructor and
// destructor. In the constructor we start the nanoapp by calling the above
// nanoapp_run on a separate thread. In the destructor, we stop the nanoapp.
//
// The initializer constructor will run before anything else in the unit test,
// and the destructor will be the last thing run in the cleanup stage.
#define NANOAPP_INITIALIZER_LOGIC_(test_case_name, test_name)               \
                                                                            \
  NANOAPP_TEST_CLASS_INITIALIZER_NAME_(                                     \
      test_case_name,                                                       \
      test_name)::NANOAPP_TEST_CLASS_INITIALIZER_NAME_(test_case_name,      \
                                                       test_name)(          \
      NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name) * test_hook) {    \
    test_case_name##_##test_name##_TestHook = test_hook;                    \
    test_hook->StartChre();                                                 \
    chre::UniquePtr<chre::Nanoapp> nanoapp =                                \
        lbs::contexthub::InitializeNanoapp(nanoapp_start##test_name,        \
                                           nanoapp_handle_event##test_name, \
                                           nanoapp_end##test_name);         \
    test_hook->LoadNanoapp(std::move(nanoapp));                             \
    test_hook->ConfigureApi();                                              \
    std::thread(nanoapp_run##test_name).detach();                           \
    test_name##init_counter.Wait();                                         \
  }                                                                         \
                                                                            \
  NANOAPP_TEST_CLASS_INITIALIZER_NAME_(                                     \
      test_case_name,                                                       \
      test_name)::~NANOAPP_TEST_CLASS_INITIALIZER_NAME_(test_case_name,     \
                                                        test_name)() {      \
    chre::EventLoopManagerSingleton::get()->getEventLoop().stop();          \
    test_name##deinit_counter.Wait();                                       \
    lbs::contexthub::FakeChreApiProvider::ResetInstance();                  \
    lbs::contexthub::FakeChrexApiProvider::ResetInstance();                 \
    test_case_name##_##test_name##_TestHook->ShutdownChre();                \
  }

// Generates a valid subclass of the given test_fixture that runs the test
// code in a simulated CHRE environment. This macro defines unique
// start/end/handle_event C functions to set up the CHRE context for the test,
// which is necessary since we cannot pass a test-specific context to them.
// All initialization and cleanup is handled by the initializer class (also a
// parent of the new subclass), so that the test_fixture constructor is run with
// a correct context and already set-up fake CHRE APIs.
//
// Below, we generates a test that:
//
// 1) Saves a hook to the subclassed test instance
// 2) Starts CHRE runtime and loads a fake nanoapp
// 3) Configures the CHRE fake APIs
// 4) Sends a message to the fake nanoapp
// 5) Starts CHRE event loop so the message can be delivered
// 6) When the nanoapp receives the message, it invokes the user defined test
//    using the hook saved earlier
// 7) After finishing the test, we set the CHRE fake APIs to an empty state0
// 8) Shuts down CHRE
//
// To understand the parallel ordering of the events, a detailed explanation can
// be found on go/nanoapp-test-concurrency.
//
// Don't use this directly! Use either TEST_NANOAPP_F or TEST_NANOAPP depending
// on whether you're using a fixture or not.
#define NANOAPP_TEST_(test_case_name, test_fixture, test_name)                \
                                                                              \
  class NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name);                  \
  NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name) *                       \
      test_case_name##_##test_name##_TestHook;                                \
                                                                              \
  class NANOAPP_TEST_CLASS_INITIALIZER_NAME_(test_case_name, test_name) {     \
   public:                                                                    \
    explicit NANOAPP_TEST_CLASS_INITIALIZER_NAME_(test_case_name, test_name)( \
        NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name) *);               \
    ~NANOAPP_TEST_CLASS_INITIALIZER_NAME_(test_case_name, test_name)();       \
  };                                                                          \
                                                                              \
  class NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name)                   \
      : public NANOAPP_TEST_CLASS_INITIALIZER_NAME_(test_case_name,           \
                                                    test_name),               \
        public test_fixture {                                                 \
    friend class NANOAPP_TEST_CLASS_INITIALIZER_NAME_(test_case_name,         \
                                                      test_name);             \
                                                                              \
   public:                                                                    \
    NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name)                       \
    ()                                                                        \
        : NANOAPP_TEST_CLASS_INITIALIZER_NAME_(test_case_name,                \
                                               test_name)(this),              \
          test_fixture() {                                                    \
      chre_api_fake_detector_ =                                               \
          lbs::contexthub::FakeChreApiProvider::GetInstance()                 \
              ->GetFakeDetector();                                            \
      chrex_api_fake_detector_ =                                              \
          lbs::contexthub::FakeChrexApiProvider::GetInstance()                \
              ->GetFakeDetector();                                            \
    }                                                                         \
    ~NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name)();                   \
    void NanoappTest();                                                       \
                                                                              \
   protected:                                                                 \
    void SetUp();                                                             \
    void TearDown();                                                          \
  };                                                                          \
                                                                              \
  void NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name)::SetUp() {         \
    test_fixture::SetUp();                                                    \
  }                                                                           \
                                                                              \
  void NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name)::TearDown() {      \
    test_fixture::TearDown();                                                 \
  }                                                                           \
                                                                              \
  absl::BlockingCounter test_name##init_counter(1);                           \
  absl::BlockingCounter test_name##deinit_counter(1);                         \
  absl::BlockingCounter test_name##start_test_counter(1);                     \
  absl::BlockingCounter test_name##end_test_counter(1);                       \
                                                                              \
  NANOAPP_TEST_FUNCTIONS_(test_case_name, test_name)                          \
  NANOAPP_INITIALIZER_LOGIC_(test_case_name, test_name)                       \
                                                                              \
  NANOAPP_TEST_CLASS_NAME_(                                                   \
      test_case_name, test_name)::~NANOAPP_TEST_CLASS_NAME_(test_case_name,   \
                                                            test_name)() {    \
    lbs::contexthub::helpers::SetGnssNoCapabilities();                        \
    lbs::contexthub::helpers::SetEmptyRe();                                   \
    lbs::contexthub::helpers::SetWifiNoCapabilities();                        \
    lbs::contexthub::helpers::SetEmptyAudio();                                \
    lbs::contexthub::helpers::SetWwanNoCapabilities();                        \
    lbs::contexthub::helpers::SetEmptySensor();                               \
    lbs::contexthub::helpers::SetEmptyEvent();                                \
    lbs::contexthub::helpers::SetEventSettingState({});                       \
  }                                                                           \
  TEST_F(NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name), NanoappTest) {  \
    test_name##start_test_counter.DecrementCount();                           \
    test_name##end_test_counter.Wait();                                       \
  }                                                                           \
                                                                              \
  void NANOAPP_TEST_CLASS_NAME_(test_case_name, test_name)::NanoappTest()

}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_UNIT_TEST_H_
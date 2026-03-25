/*
 * Copyright (C) 2026 The Android Open Source Project
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

#include "gtest/gtest.h"

#include "chre/core/nanoapp.h"
#include "chre/core/wakeup_stats_manager.h"

using chre::Nanoapp;
using chre::WakeupReason;
using chre::WakeupStatsManager;

namespace {
constexpr uint16_t kNanoappInstanceId = 0x1234;
}  // namespace

TEST(WakeupStatsManager, InitialState) {
  WakeupStatsManager manager;
  EXPECT_FALSE(manager.isHostWakeupBlamed());
}

TEST(WakeupStatsManager, BlameWakeupWithNullNanoapp) {
  WakeupStatsManager manager;
  manager.blameWakeup(nullptr, WakeupReason::NANOAPP_MESSAGE);
  EXPECT_TRUE(manager.isHostWakeupBlamed());
}

TEST(WakeupStatsManager, BlameWakeupWithNanoapp) {
  WakeupStatsManager manager;
  Nanoapp nanoapp(kNanoappInstanceId);

  // Nanoapps need to have at least one wakeup bucket to record wakeups.
  nanoapp.cycleWakeupBuckets(chre::Nanoseconds(100));

  EXPECT_EQ(nanoapp.getWakeupCountSinceBoot(), 0u);

  manager.blameWakeup(&nanoapp, WakeupReason::NANOAPP_MESSAGE);
  EXPECT_TRUE(manager.isHostWakeupBlamed());
  EXPECT_EQ(nanoapp.getWakeupCountSinceBoot(), 1u);
}

TEST(WakeupStatsManager, BlameOnlyOncePerCycle) {
  WakeupStatsManager manager;
  Nanoapp nanoapp1(kNanoappInstanceId);
  Nanoapp nanoapp2(kNanoappInstanceId + 1);

  nanoapp1.cycleWakeupBuckets(chre::Nanoseconds(100));
  nanoapp2.cycleWakeupBuckets(chre::Nanoseconds(100));

  manager.blameWakeup(&nanoapp1, WakeupReason::NANOAPP_MESSAGE);
  EXPECT_TRUE(manager.isHostWakeupBlamed());
  EXPECT_EQ(nanoapp1.getWakeupCountSinceBoot(), 1u);

  // Second blame should be ignored
  manager.blameWakeup(&nanoapp2, WakeupReason::NANOAPP_MESSAGE);
  EXPECT_TRUE(manager.isHostWakeupBlamed());
  EXPECT_EQ(nanoapp2.getWakeupCountSinceBoot(), 0u);
}

TEST(WakeupStatsManager, ResetBlameClearsFlag) {
  WakeupStatsManager manager;
  manager.blameWakeup(nullptr, WakeupReason::NANOAPP_MESSAGE);
  EXPECT_TRUE(manager.isHostWakeupBlamed());

  manager.resetBlameForHostWakeup();
  EXPECT_FALSE(manager.isHostWakeupBlamed());
}

TEST(WakeupStatsManager, MultipleCycles) {
  WakeupStatsManager manager;
  Nanoapp nanoapp(kNanoappInstanceId);
  nanoapp.cycleWakeupBuckets(chre::Nanoseconds(100));

  // Cycle 1
  manager.blameWakeup(&nanoapp, WakeupReason::NANOAPP_MESSAGE);
  EXPECT_TRUE(manager.isHostWakeupBlamed());
  EXPECT_EQ(nanoapp.getWakeupCountSinceBoot(), 1u);

  // Reset for Cycle 2
  manager.resetBlameForHostWakeup();
  EXPECT_FALSE(manager.isHostWakeupBlamed());

  // Cycle 2
  manager.blameWakeup(&nanoapp, WakeupReason::NANOAPP_MESSAGE);
  EXPECT_TRUE(manager.isHostWakeupBlamed());
  EXPECT_EQ(nanoapp.getWakeupCountSinceBoot(), 2u);
}

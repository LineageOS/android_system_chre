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

#include "data_flow/host/region_manager.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <vector>

#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <android-base/unique_fd.h>

#include "gtest/gtest.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::EndpointId;
using ::aidl::android::hardware::contexthub::SharedDataRegion;

class RegionManagerTest : public ::testing::Test {
 protected:
  pw::Result<RegionManager::RegionToMap> createRegionToMap(size_t size,
                                                           bool writable) {
    PW_TRY_ASSIGN(auto region, createSharedDataRegion(size, writable));
    return RegionManager::RegionToMap{
        .id = region.id,
        .fd = std::move(region.sharedMemory),
        .size = static_cast<size_t>(region.size),
    };
  }

  pw::Result<SharedDataRegion> createSharedDataRegion(size_t size,
                                                      bool writable) {
    std::string name = "testFd" + std::to_string(mNextRegionId);
    android::base::unique_fd fd(
        syscall(SYS_memfd_create, name.c_str(), MFD_ALLOW_SEALING));
    if (fd.get() < 0) {
      return pw::Status::Internal();
    }
    if (ftruncate(fd.get(), size) != 0) {
      return pw::Status::Internal();
    };
    int seals = F_SEAL_GROW | F_SEAL_SHRINK;
    if (!writable) {
      // Use F_SEAL_FUTURE_WRITE instead of F_SEAL_WRITE due to a kernel bug
      // that rejects MAP_SHARED PROT_READ mmap()s under the assumption that
      // MAP_SHARED implies an intention to write. It has since been fixed, but
      // not sure if that has necessarily propagated to all test environments.
      // https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=e8e17ee90eaf650c855adb0a3e5e965fd6692ff1
      seals |= F_SEAL_FUTURE_WRITE;
    }
    if (fcntl(fd.get(), F_ADD_SEALS, seals) != 0) {
      return pw::Status::Internal();
    }
    return SharedDataRegion{
        .id = mNextRegionId++,
        .sharedMemory = ndk::ScopedFileDescriptor(fd.release()),
        .size = static_cast<uint32_t>(size)};
  }

  RegionManager mManager;
  int mNextRegionId = 1;
};

TEST_F(RegionManagerTest, MapAndUnmapHostProducerRegion) {
  auto regionToMap = createRegionToMap(1024, /*writable=*/true);
  ASSERT_EQ(regionToMap.status(), pw::OkStatus());
  int id = regionToMap->id;

  auto region = mManager.mapHostProducerRegion(std::move(*regionToMap));
  ASSERT_EQ(region.status(), pw::OkStatus());
  EXPECT_TRUE(region->allocator);
  EXPECT_GE(region->size, 1024);
  if (region->allocator && region->size >= 1024) {
    auto *ptr = region->allocator->New<int>();
    ASSERT_TRUE(ptr);
    *ptr = 42;
    region->allocator->Delete(ptr);
  }

  auto gotRegion = mManager.getHostProducerRegion(id);
  ASSERT_EQ(gotRegion.status(), pw::OkStatus());
  EXPECT_EQ(gotRegion->base, region->base);
  EXPECT_EQ(gotRegion->size, region->size);
  EXPECT_EQ(gotRegion->allocator, region->allocator);

  EXPECT_EQ(mManager.unmapHostProducerRegion(id), pw::OkStatus());
}

TEST_F(RegionManagerTest, MapHostProducerRegionFromSharedDataRegion) {
  auto sharedDataRegion = createSharedDataRegion(1024, /*writable=*/true);
  ASSERT_EQ(sharedDataRegion.status(), pw::OkStatus());

  auto region = mManager.mapHostProducerRegion(*sharedDataRegion);
  ASSERT_EQ(region.status(), pw::OkStatus());
  EXPECT_TRUE(region->allocator);
  EXPECT_GE(region->size, 1024);
  if (region->allocator && region->size >= 1024) {
    auto *ptr = region->allocator->New<int>();
    ASSERT_TRUE(ptr);
    *ptr = 42;
    region->allocator->Delete(ptr);
  }
  EXPECT_EQ(mManager.unmapHostProducerRegion(sharedDataRegion->id),
            pw::OkStatus());
}

TEST_F(RegionManagerTest, UnmapHostProducerRegionInUse) {
  auto regionToMap = createRegionToMap(1024, /*writable=*/true);
  ASSERT_EQ(regionToMap.status(), pw::OkStatus());
  int regionId = regionToMap->id;

  ASSERT_EQ(mManager.mapHostProducerRegion(std::move(*regionToMap)).status(),
            pw::OkStatus());

  int dataFlowId = 123;
  EXPECT_EQ(mManager.linkHostProducerDataFlowToRegion(regionId, dataFlowId),
            pw::OkStatus());

  EXPECT_EQ(mManager.unmapHostProducerRegion(regionId),
            pw::Status::FailedPrecondition());

  ASSERT_EQ(mManager.unlinkHostProducerDataFlow(dataFlowId).status(),
            pw::OkStatus());
  EXPECT_EQ(mManager.unmapHostProducerRegion(regionId), pw::OkStatus());
}

TEST_F(RegionManagerTest, MapHostProducerRegionAlreadyExists) {
  auto regionToMap1 = createRegionToMap(1024, /*writable=*/true);
  ASSERT_EQ(regionToMap1.status(), pw::OkStatus());
  int id = regionToMap1->id;

  auto regionToMap2 = createRegionToMap(1024, /*writable=*/true);
  ASSERT_EQ(regionToMap2.status(), pw::OkStatus());
  regionToMap2->id = id;

  ASSERT_EQ(mManager.mapHostProducerRegion(std::move(*regionToMap1)).status(),
            pw::OkStatus());

  EXPECT_EQ(mManager.mapHostProducerRegion(std::move(*regionToMap2)).status(),
            pw::Status::AlreadyExists());

  EXPECT_EQ(mManager.unmapHostProducerRegion(id), pw::OkStatus());
}

TEST_F(RegionManagerTest, LinkAndUnlinkHostProducerDataFlow) {
  auto regionToMap = createRegionToMap(1024, /*writable=*/true);
  ASSERT_EQ(regionToMap.status(), pw::OkStatus());
  int regionId = regionToMap->id;

  ASSERT_EQ(mManager.mapHostProducerRegion(std::move(*regionToMap)).status(),
            pw::OkStatus());

  int dataFlowId = 123;
  EXPECT_EQ(mManager.linkHostProducerDataFlowToRegion(regionId, dataFlowId),
            pw::OkStatus());
  EXPECT_EQ(mManager.linkHostProducerDataFlowToRegion(regionId, dataFlowId),
            pw::Status::AlreadyExists());

  auto result = mManager.unlinkHostProducerDataFlow(dataFlowId);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(*result, 0);

  EXPECT_EQ(mManager.unlinkHostProducerDataFlow(dataFlowId).status(),
            pw::Status::NotFound());

  EXPECT_EQ(mManager.unmapHostProducerRegion(regionId), pw::OkStatus());
}

TEST_F(RegionManagerTest, MapOffloadConsumerRegionWithInvalidDataFlow) {
  auto consumerRegion = createRegionToMap(1024, /*writable=*/true);
  ASSERT_EQ(consumerRegion.status(), pw::OkStatus());
  EndpointId consumerId = {.hubId = 1, .id = 1};
  int nonExistentDataFlowId = 999;

  EXPECT_EQ(mManager
                .mapOffloadConsumerRegion(std::move(*consumerRegion),
                                          consumerId, nonExistentDataFlowId)
                .status(),
            pw::Status::FailedPrecondition());
}

TEST_F(RegionManagerTest, MapAndPruneOffloadConsumerRegion) {
  auto hostProducerRegion = createRegionToMap(4096, /*writable=*/true);
  ASSERT_EQ(hostProducerRegion.status(), pw::OkStatus());
  int producerRegionId = hostProducerRegion->id;
  ASSERT_EQ(
      mManager.mapHostProducerRegion(std::move(*hostProducerRegion)).status(),
      pw::OkStatus());

  int dataFlowId = 1;
  ASSERT_EQ(
      mManager.linkHostProducerDataFlowToRegion(producerRegionId, dataFlowId),
      pw::OkStatus());

  auto consumerRegion = createRegionToMap(1024, /*writable=*/true);
  ASSERT_EQ(consumerRegion.status(), pw::OkStatus());
  EndpointId consumerId = {.hubId = 1, .id = 1};

  auto mappedConsumerRegion = mManager.mapOffloadConsumerRegion(
      std::move(*consumerRegion), consumerId, dataFlowId);
  ASSERT_EQ(mappedConsumerRegion.status(), pw::OkStatus());
  auto *ptr = mappedConsumerRegion->allocator->New<int>();
  ASSERT_TRUE(ptr);
  *ptr = 13;
  mappedConsumerRegion->allocator->Delete(ptr);

  mManager.pruneOffloadConsumer(consumerId);

  auto result = mManager.unlinkHostProducerDataFlow(dataFlowId);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(*result, 0);

  EXPECT_EQ(mManager.unmapHostProducerRegion(producerRegionId), pw::OkStatus());
}

TEST_F(RegionManagerTest, MapAndUnlinkHostConsumerRegions) {
  auto region = createRegionToMap(1024, /*writable=*/true);
  ASSERT_EQ(region.status(), pw::OkStatus());
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};

  auto regions = mManager.mapHostConsumerRegions(std::move(*region),
                                                 std::nullopt, dataFlowId);
  ASSERT_EQ(regions.status(), pw::OkStatus());
  EXPECT_FALSE(regions->second.has_value());

  EXPECT_EQ(mManager.unlinkHostConsumerDataFlow(dataFlowId), pw::OkStatus());
  EXPECT_EQ(mManager.unlinkHostConsumerDataFlow(dataFlowId),
            pw::Status::NotFound());
}

TEST_F(RegionManagerTest, MapAndUnlinkHostConsumerRegionsWithMetadataRegion) {
  auto region = createRegionToMap(1024, /*writable=*/false);
  ASSERT_EQ(region.status(), pw::OkStatus());
  auto metadataRegion = createRegionToMap(256, /*writable=*/true);
  ASSERT_EQ(metadataRegion.status(), pw::OkStatus());
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};

  auto regions = mManager.mapHostConsumerRegions(
      std::move(*region), std::move(*metadataRegion), dataFlowId);
  ASSERT_EQ(regions.status(), pw::OkStatus());
  EXPECT_TRUE(regions->second.has_value());

  EXPECT_EQ(mManager.unlinkHostConsumerDataFlow(dataFlowId), pw::OkStatus());
}

}  // namespace
}  // namespace android::contexthub::data_flow
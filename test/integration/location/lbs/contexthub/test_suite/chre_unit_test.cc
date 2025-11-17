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

#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

#include <utility>

#include <gtest/gtest.h>
#include "chre/platform/linux/platform_log.h"
#include "chre/platform/shared/init.h"
#include "chre/util/system/napp_permissions.h"

namespace lbs {
namespace contexthub {

chre::UniquePtr<chre::Nanoapp> InitializeNanoapp(
    decltype(nanoappStart) *start, decltype(nanoappHandleEvent) *handle_event,
    decltype(nanoappEnd) *end) {
  chre::UniquePtr<chre::Nanoapp> nanoapp = chre::MakeUnique<chre::Nanoapp>();
  static struct chreNslNanoappInfo app_info;
  app_info.magic = CHRE_NSL_NANOAPP_INFO_MAGIC;
  app_info.structMinorVersion = CHRE_NSL_NANOAPP_INFO_STRUCT_MINOR_VERSION;
  app_info.targetApiVersion = CHRE_API_VERSION;
  app_info.vendor = "Google";
  app_info.name = "Test";
  app_info.isSystemNanoapp = true;
  app_info.isTcmNanoapp = false;
  app_info.appId = 0x2000;
  app_info.appVersion = 0x1;
  app_info.entryPoints.start = start;
  app_info.entryPoints.handleEvent = handle_event;
  app_info.entryPoints.end = end;
  app_info.appPermissions = chre::NanoappPermissions::CHRE_PERMS_ALL;
  if (nanoapp.isNull()) {
    FATAL_ERROR("Failed to allocate nanoapp for test");
  } else {
    nanoapp->loadStatic(&app_info);
  }

  return nanoapp;
}

void ChreUnitTest::StartChre() {
  // Initialize logging.
  chre::PlatformLogSingleton::init();

  // Initialize the system.
  chre::initCommon();
}

void ChreUnitTest::LoadNanoapp(chre::UniquePtr<chre::Nanoapp> &&nanoapp) {
  chre::EventLoopManagerSingleton::get()->getEventLoop().startNanoapp(
      std::move(nanoapp));
}

void ChreUnitTest::ShutdownChre() {
  chre::deinitCommon();
  chre::PlatformLogSingleton::deinit();
}

}  // namespace contexthub
}  // namespace lbs

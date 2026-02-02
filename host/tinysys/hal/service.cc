/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "tinysys_context_hub.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "bluetooth_socket_fbs_hal.h"
#include "aidl/android/hardware/bluetooth/socket/BnBluetoothSocket.h"

#ifndef LOG_TAG
#define LOG_TAG "android.hardware.contexthub-service"
#endif

using aidl::android::hardware::contexthub::TinysysContextHub;
using aidl::android::hardware::bluetooth::socket::impl::BluetoothSocketFbsHal;
using aidl::android::hardware::bluetooth::socket::IBluetoothSocket;

int main() {
  ABinderProcess_setThreadPoolMaxThreadCount(0);

  // Make a default contexthub service
  auto contextHub = ndk::SharedRefBase::make<TinysysContextHub>();
  const std::string contextHubName =
      std::string() + TinysysContextHub::descriptor + "/default";
  binder_status_t status = AServiceManager_addService(
      contextHub->asBinder().get(), contextHubName.c_str());
  CHECK(status == STATUS_OK);

  std::string bluetoothSocketName = std::string() + IBluetoothSocket::descriptor + "/lpp";
  if(AServiceManager_isDeclared(bluetoothSocketName.c_str()))
  {
      LOGI("Starting Bluetooth Socket HAL");
      auto bluetoothSocket = ndk::SharedRefBase::make<BluetoothSocketFbsHal>(
          contextHub->getBluetoothSocketOffloadLink());
      status = AServiceManager_addService(bluetoothSocket->asBinder().get(),
                                          bluetoothSocketName.c_str());
  } else {
    LOGI("bluetoothSocketName not declared!");
  }

  ABinderProcess_joinThreadPool();
  return EXIT_FAILURE;  // should not reach
}

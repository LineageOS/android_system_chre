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

#ifndef CHRE_CORE_DATA_FLOW_MANAGER_H_
#define CHRE_CORE_DATA_FLOW_MANAGER_H_

#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED

#include "chre/util/non_copyable.h"

namespace chre {

//! Manager class for data flow support in CHRE.
class DataFlowManager : public NonCopyable {
 public:
  DataFlowManager() = default;
  ~DataFlowManager() = default;

  //! Initializes the DataFlowManager.
  void init();
};

}  // namespace chre

#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED

#endif  // CHRE_CORE_DATA_FLOW_MANAGER_H_

/* * Copyright (C) 2020 The Android Open Source Project
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

#include "chre_cross_validator_wifi_manager.h"

#include <cinttypes>
#include <cstring>

#include "chre/util/nanoapp/log.h"
#include "chre/util/nanoapp/wifi.h"
#include "chre/util/time.h"
#include "chre_api/chre.h"
#include "send_message.h"

namespace chre::cross_validator_wifi {

// Fake scan monitor cookie which is not used
constexpr uint32_t kScanMonitoringCookie = 0;

// The cookie to use for the data collection timeout timer
constexpr uint32_t kTimeoutTimerCookie = 0x1337;

// The timeout for data collection. The host side timeout is 15 seconds, so we
// set this to be shorter than that to avoid stalling the host.
// Note that this timeout is only for gathering more scan events if the first
// compare attempt failed.
constexpr uint64_t kDataCollectionTimeoutNs = 5 * chre::kOneSecondInNanoseconds;

void Manager::handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                          const void *eventData) {
  switch (eventType) {
    case CHRE_EVENT_MESSAGE_FROM_HOST:
      handleMessageFromHost(
          senderInstanceId,
          static_cast<const chreMessageFromHostData *>(eventData));
      break;
    case CHRE_EVENT_WIFI_ASYNC_RESULT:
      handleWifiAsyncResult(static_cast<const chreAsyncResult *>(eventData));
      break;
    case CHRE_EVENT_WIFI_SCAN_RESULT:
      handleWifiScanResult(static_cast<const chreWifiScanEvent *>(eventData));
      break;
    case CHRE_EVENT_TIMER:
      handleTimerEvent(eventData);
      break;
    default:
      LOGE("Unknown message type %" PRIu16 "received when handling event",
           eventType);
  }
}

void Manager::handleMessageFromHost(uint32_t senderInstanceId,
                                    const chreMessageFromHostData *hostData) {
  if (senderInstanceId != CHRE_INSTANCE_ID) {
    LOGE("Incorrect sender instance id: %" PRIu32, senderInstanceId);
    return;
  }
  mCrossValidatorState.hostEndpoint = hostData->hostEndpoint;
  switch (hostData->messageType) {
    case chre_cross_validation_wifi_MessageType_STEP_START: {
      pb_istream_t stream = pb_istream_from_buffer(
          static_cast<const pb_byte_t *>(
              const_cast<const void *>(hostData->message)),
          hostData->messageSize);
      chre_cross_validation_wifi_StepStartCommand stepStartCommand;
      if (!pb_decode(&stream,
                     chre_cross_validation_wifi_StepStartCommand_fields,
                     &stepStartCommand)) {
        LOGE("Error decoding StepStartCommand");
        break;
      }
      handleStepStartMessage(stepStartCommand);
      break;
    }
    case chre_cross_validation_wifi_MessageType_SCAN_RESULT:
      handleDataMessage(hostData);
      break;
    default:
      LOGE("Unknown message type %" PRIu32 " for host message",
           hostData->messageType);
  }
}

void Manager::handleStepStartMessage(
    chre_cross_validation_wifi_StepStartCommand stepStartCommand) {
  switch (stepStartCommand.step) {
    case chre_cross_validation_wifi_Step_INIT:
      LOGE("Received StepStartCommand for INIT step");
      break;
    case chre_cross_validation_wifi_Step_CAPABILITIES: {
      LOGD("%s: Received Step_CAPABILITIES", __func__);
      chre_cross_validation_wifi_WifiCapabilities wifiCapabilities =
          makeWifiCapabilitiesMessage(chreWifiGetCapabilities());
      test_shared::sendMessageToHost(
          mCrossValidatorState.hostEndpoint, &wifiCapabilities,
          chre_cross_validation_wifi_WifiCapabilities_fields,
          chre_cross_validation_wifi_MessageType_WIFI_CAPABILITIES);
      break;
    }
    case chre_cross_validation_wifi_Step_SETUP: {
      if (!chreWifiConfigureScanMonitorAsync(/* enable= */ true,
                                             &kScanMonitoringCookie)) {
        LOGE("chreWifiConfigureScanMonitorAsync() failed");
        sendTestResult(/*success=*/false, "setupWifiScanMonitoring failed");
        break;
      }
      LOGD("chreWifiConfigureScanMonitorAsync() succeeded");
      if (stepStartCommand.has_chreScanCapacity) {
        mExpectedMaxChreResultCanHandle = stepStartCommand.chreScanCapacity;
      }
      break;
    }
    case chre_cross_validation_wifi_Step_VALIDATE:
      break;
    default:
      LOGE("Unexpected start step: %" PRIu8,
           static_cast<uint8_t>(stepStartCommand.step));
  }
  mStep = stepStartCommand.step;
}

void Manager::handleDataMessage(const chreMessageFromHostData *hostData) {
  pb_istream_t stream =
      pb_istream_from_buffer(reinterpret_cast<const pb_byte_t *>(
                                 const_cast<const void *>(hostData->message)),
                             hostData->messageSize);
  WifiScanResult scanResult(&stream);
  uint8_t scanResultIndex = scanResult.getResultIndex();
  if (scanResultIndex > scanResult.getTotalNumResults()) {
    LOGE("AP scan result index is greater than scan results size");
    return;
  }
  if (!mApScanResults.push_back(scanResult)) {
    LOG_OOM();
    sendTestResult(/*success=*/false, "OOM handling AP scan results");
  }
  LOGD("%s: AP wifi result %" PRIu8 "/%" PRIu8 " is received", __func__,
       static_cast<uint8_t>(scanResultIndex + 1),
       scanResult.getTotalNumResults());
  if (!scanResult.isLastMessage()) {
    return;
  }
  mApDataCollectionDone = true;
  if (mChreDataCollectionDone) {
    compareAndSendResultToHost();
  }
}

void Manager::handleWifiScanResult(const chreWifiScanEvent *event) {
  if (event->eventIndex == 0) {
    mScanStartSeen = true;
    if (!mChreScanResults.emplace_back()) {
      LOG_OOM();
      sendTestResult(/*success=*/false, "OOM handling CHRE scan batch");
      return;
    }
  }

  if (!mScanStartSeen) {
    LOGW("Dropping chreWifiScanEvent because we haven't seen eventIndex=0");
    return;
  }

  auto &currentBatch = mChreScanResults.back();
  for (uint8_t i = 0; i < event->resultCount; i++) {
    if (!currentBatch.push_back(event->results[i])) {
      LOG_OOM();
      sendTestResult(/*success=*/false, "OOM handling CHRE scan result");
      return;
    }
  }

  LOGI("%s: CHRE wifi result %zu/%" PRIu8 " is received", __func__,
       currentBatch.size(), event->resultTotal);

  if (currentBatch.size() < event->resultTotal) {
    return;
  }

  mChreDataCollectionDone = true;
  if (mApDataCollectionDone) {
    compareAndSendResultToHost();
  }
}

void Manager::compareAndSendResultToHost() {
  if (verifyScanResults()) {
    if (mTimeoutTimerHandle != CHRE_TIMER_INVALID) {
      chreTimerCancel(mTimeoutTimerHandle);
      mTimeoutTimerHandle = CHRE_TIMER_INVALID;
    }
    sendTestResult(/*success=*/true);
  } else {
    // Start the timer to wait for more data if we haven't already.
    if (mTimeoutTimerHandle == CHRE_TIMER_INVALID) {
      mTimeoutTimerHandle =
          chreTimerSet(kDataCollectionTimeoutNs, &kTimeoutTimerCookie, true);
    }
    LOGW("Verification failed for current batch(es). Waiting for more data...");
  }
}

bool Manager::verifyScanResults() {
  // Multi-batch verification: Nanoapp might receive scan events from scans
  // triggered other than the test host. We consider the test a PASS if ANY
  // single batch matches the Host's scan results.
  for (size_t i = mNumChreScanResultsProcessed; i < mChreScanResults.size();
       ++i) {
    const auto &batch = mChreScanResults[i];
    LOGI("Verifying batch %zu", i);
    for (size_t j = 0; j < mApScanResults.size(); ++j) {
      mApScanResults[j].resetSeen();
    }

    // We run all verification steps even if one fails to ensure that all
    // relevant error information is logged for debugging.
    bool countsMatch = verifyScanResultCounts(mApScanResults, batch,
                                              mExpectedMaxChreResultCanHandle);
    bool chreMatchesAp = verifyChreResultsMatchAp(batch, mApScanResults);
    bool apSeenInChre = verifyApResultsSeenInChre(mApScanResults);

    if (countsMatch && chreMatchesAp && apSeenInChre) {
      return true;
    }
  }

  // If we reach here, no current batch matched.
  mNumChreScanResultsProcessed = mChreScanResults.size();
  return false;
}

bool Manager::verifyScanResultCounts(
    const DynamicVector<WifiScanResult> &apResults,
    const DynamicVector<chreWifiScanResult> &chreBatch, uint8_t maxExpected) {
  bool isExactCountMismatch =
      apResults.size() <= maxExpected && apResults.size() != chreBatch.size();
  // If AP results exceed the max CHRE can handle, CHRE must not return *more*
  // results than AP (it's okay to return fewer due to capacity limits).
  bool isChreCountLargerThanAp =
      apResults.size() > maxExpected && apResults.size() < chreBatch.size();

  LOGI("Wifi scan result counts, AP = %zu, CHRE = %zu, MAX = %" PRIu8,
       apResults.size(), chreBatch.size(), maxExpected);

  if (isExactCountMismatch || isChreCountLargerThanAp) {
    LOGW("Scan results differ: AP = %zu, CHRE = %zu, MAX = %" PRIu8,
         apResults.size(), chreBatch.size(), maxExpected);
    setLastError("Scan results count mismatch");
    return false;
  }
  return true;
}

bool Manager::verifyChreResultsMatchAp(
    const DynamicVector<chreWifiScanResult> &chreBatch,
    DynamicVector<WifiScanResult> &apResults) {
  bool batchSuccess = true;
  for (const chreWifiScanResult &result : chreBatch) {
    const WifiScanResult chreScanResult = WifiScanResult(result);
    bool isValidResult = true;
    size_t index = getMatchingScanResultIndex(apResults, chreScanResult);

    const char *bssidStr = "<non-printable>";
    char bssidBuffer[chre::kBssidStrLen];
    if (chre::parseBssidToStr(chreScanResult.getBssid(), bssidBuffer,
                              sizeof(bssidBuffer))) {
      bssidStr = bssidBuffer;
    }

    if (index != SIZE_MAX) {
      WifiScanResult &apScanResult = apResults[index];
      if (apScanResult.getSeen()) {
        isValidResult = false;
        LOGW("CHRE Scan Result with bssid: %s has a duplicate BSSID", bssidStr);
        setLastError("Duplicate BSSID in CHRE results");
      }
      if (!WifiScanResult::areEqual(chreScanResult, apScanResult)) {
        isValidResult = false;
        LOGW(
            "CHRE Scan Result with bssid: %s ssid: %s found fields differ "
            "with an AP scan result with same Bssid",
            bssidStr, chreScanResult.getSsid());
        setLastError("CHRE/AP scan result fields differ");
      }
      apScanResult.didSee();
    } else {
      isValidResult = false;
      LOGW(
          "CHRE Scan Result with bssid: %s fail to find an AP scan "
          "with same Bssid",
          bssidStr);
      setLastError("CHRE scan result BSSID not found in AP results");
    }

    if (!isValidResult) {
      LOGW("False CHRE Scan Result with the following info:");
      logChreWifiResult(result);
      batchSuccess = false;
    }
  }
  return batchSuccess;
}

bool Manager::verifyApResultsSeenInChre(
    const DynamicVector<WifiScanResult> &apResults) {
  bool batchSuccess = true;
  for (const WifiScanResult &scanResult : apResults) {
    if (!scanResult.getSeen()) {
      const char *bssidStr = "<non-printable>";
      char bssidBuffer[chre::kBssidStrLen];
      if (chre::parseBssidToStr(scanResult.getBssid(), bssidBuffer,
                                sizeof(bssidBuffer))) {
        bssidStr = bssidBuffer;
      }
      LOGW("AP %s with bssid %s is not seen in CHRE", scanResult.getSsid(),
           bssidStr);
      // Since CHRE is more constrained in memory, it is expected that if we
      // receive over a certain amount of AP, we will drop some of them.
      if (apResults.size() <= mExpectedMaxChreResultCanHandle) {
        LOGW(
            "Extra AP information shown in host "
            "when small number of AP results presenting");
        setLastError("AP result not seen in CHRE");
        batchSuccess = false;
      }
    }
  }
  return batchSuccess;
}

size_t Manager::getMatchingScanResultIndex(
    const DynamicVector<WifiScanResult> &results,
    const WifiScanResult &queryResult) {
  for (size_t i = 0; i < results.size(); i++) {
    if (WifiScanResult::bssidsAreEqual(results[i], queryResult)) {
      return i;
    }
  }
  return SIZE_MAX;
}

void Manager::sendTestResult(bool success, const char *errorMessage) const {
  test_shared::sendTestResultWithMsgToHost(
      mCrossValidatorState.hostEndpoint,
      chre_cross_validation_wifi_MessageType_STEP_RESULT, success, errorMessage,
      /* abortOnFailure= */ false);
}

chre_cross_validation_wifi_WifiCapabilities
Manager::makeWifiCapabilitiesMessage(uint32_t capabilitiesFromChre) {
  chre_cross_validation_wifi_WifiCapabilities capabilities;
  capabilities.has_wifiCapabilities = true;
  capabilities.wifiCapabilities = capabilitiesFromChre;
  return capabilities;
}

void Manager::handleWifiAsyncResult(const chreAsyncResult *result) {
  if (result->requestType != CHRE_WIFI_REQUEST_TYPE_CONFIGURE_SCAN_MONITOR) {
    sendTestResult(/*success=*/false,
                   /*errorMessage=*/"Unknown chre async result type received");
    return;
  }
  if (mStep != chre_cross_validation_wifi_Step_SETUP) {
    sendTestResult(
        /*success=*/false,
        /*errorMessage=*/"Received scan monitor result but step is not SETUP");
    return;
  }
  if (result->success) {
    LOGI("Wifi scan monitoring setup successfully");
    mTimeoutTimerHandle =
        chreTimerSet(kDataCollectionTimeoutNs, &kTimeoutTimerCookie, true);
    sendTestResult(/*success=*/true);
  } else {
    LOGE("Wifi scan monitoring setup failed async w/ error code %" PRIu8,
         result->errorCode);
    sendTestResult(/*success=*/false,
                   /*errorMessage=*/"Wifi scan monitoring setup failed async");
  }
}

void Manager::handleTimerEvent(const void *eventData) {
  if (eventData == &kTimeoutTimerCookie) {
    LOGE("Timeout waiting for more wifi scan results");
    if (mLastErrorMsg[0] == '\0') {
      setLastError("Timeout waiting for more wifi scan results");
    }
    sendTestResult(/*success=*/false, mLastErrorMsg);
  }
}

void Manager::setLastError(const char *errorMsg) {
  strncpy(mLastErrorMsg, errorMsg, kMaxErrorMsgLen);
  mLastErrorMsg[kMaxErrorMsgLen - 1] = '\0';
}

}  // namespace chre::cross_validator_wifi
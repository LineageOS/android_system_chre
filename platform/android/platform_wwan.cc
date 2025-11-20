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

#include "chre/platform/platform_wwan.h"

#include <cstdint>
#include "chre/core/event_loop_manager.h"
#include "chre/core/wwan_request_manager.h"
#include "chre/pal/wwan.h"
#include "chre/platform/android/jni_manager.h"
#include "chre/platform/log.h"
#include "chre_api/chre/wwan.h"
#include "jni.h"

/**
 * An implementation of the WWAN for the CHRE AP platform.
 */
namespace chre {
namespace {

JNIEnv *getEnv() {
  return JniManagerSingleton::get()->getOrAttachJniEnv();
}

JniManager::WwanJniCache &getWwanJniCache() {
  return JniManagerSingleton::get()->mWwanJniCache;
}

// Helper to convert Java String MCC/MNC (used in NR) to int
static int32_t parseStringToInt(JNIEnv *env, jstring str) {
  if (!str) return INT32_MAX;

  const char *chars = env->GetStringUTFChars(str, nullptr);
  int32_t val = INT32_MAX;

  if (chars) {
    char *endPtr;
    long parsedVal = strtol(chars, &endPtr, 10);

    if (endPtr != chars && parsedVal >= 0 && parsedVal <= INT32_MAX) {
      val = static_cast<int32_t>(parsedVal);
    }

    env->ReleaseStringUTFChars(str, chars);
  }
  return val;
}

// Common Fields Populator
static void populateCommonFields(JNIEnv *env, jobject cellInfoObj,
                                 chreWwanCellInfo *outInfo) {
  outInfo->timeStamp =
      env->CallLongMethod(cellInfoObj, getWwanJniCache().getTimeStamp);
  outInfo->timeStampType = CHRE_WWAN_CELL_TIMESTAMP_TYPE_MODEM;

  jboolean registered =
      env->CallBooleanMethod(cellInfoObj, getWwanJniCache().isRegistered);
  outInfo->registered = (registered == JNI_TRUE) ? 1 : 0;

  outInfo->reserved = 0;
}

// GSM Parser
static void parseGsmInfo(JNIEnv *env, jobject cellInfo,
                         chreWwanCellInfo *outInfo) {
  outInfo->cellInfoType = CHRE_WWAN_CELL_INFO_TYPE_GSM;

  jobject id =
      env->CallObjectMethod(cellInfo, getWwanJniCache().gsmGetIdentity);
  jobject sig = env->CallObjectMethod(cellInfo, getWwanJniCache().gsmGetSignal);

  if (id) {
    outInfo->CellInfo.gsm.cellIdentityGsm.mcc =
        env->CallIntMethod(id, getWwanJniCache().gsmIdGetMcc);
    outInfo->CellInfo.gsm.cellIdentityGsm.mnc =
        env->CallIntMethod(id, getWwanJniCache().gsmIdGetMnc);
    outInfo->CellInfo.gsm.cellIdentityGsm.lac =
        env->CallIntMethod(id, getWwanJniCache().gsmIdGetLac);
    outInfo->CellInfo.gsm.cellIdentityGsm.cid =
        env->CallIntMethod(id, getWwanJniCache().gsmIdGetCid);
    outInfo->CellInfo.gsm.cellIdentityGsm.arfcn =
        env->CallIntMethod(id, getWwanJniCache().gsmIdGetArfcn);
    outInfo->CellInfo.gsm.cellIdentityGsm.bsic =
        env->CallIntMethod(id, getWwanJniCache().gsmIdGetBsic);
    env->DeleteLocalRef(id);
  } else {
    // Fill with invalid/defaults if identity is missing
    outInfo->CellInfo.gsm.cellIdentityGsm.mcc = INT32_MAX;
  }

  if (sig) {
    outInfo->CellInfo.gsm.signalStrengthGsm.signalStrength =
        env->CallIntMethod(sig, getWwanJniCache().gsmSigGetDbm);
    outInfo->CellInfo.gsm.signalStrengthGsm.bitErrorRate =
        env->CallIntMethod(sig, getWwanJniCache().gsmSigGetBitErrorRate);
    // GSM Timing Advance often not available in standard API
    outInfo->CellInfo.gsm.signalStrengthGsm.timingAdvance = INT32_MAX;
    env->DeleteLocalRef(sig);
  }
}

// LTE Parser
static void parseLteInfo(JNIEnv *env, jobject cellInfo,
                         chreWwanCellInfo *outInfo) {
  outInfo->cellInfoType = CHRE_WWAN_CELL_INFO_TYPE_LTE;

  jobject id =
      env->CallObjectMethod(cellInfo, getWwanJniCache().lteGetIdentity);
  jobject sig = env->CallObjectMethod(cellInfo, getWwanJniCache().lteGetSignal);

  if (id) {
    outInfo->CellInfo.lte.cellIdentityLte.mcc =
        env->CallIntMethod(id, getWwanJniCache().lteIdGetMcc);
    outInfo->CellInfo.lte.cellIdentityLte.mnc =
        env->CallIntMethod(id, getWwanJniCache().lteIdGetMnc);
    outInfo->CellInfo.lte.cellIdentityLte.ci =
        env->CallIntMethod(id, getWwanJniCache().lteIdGetCi);
    outInfo->CellInfo.lte.cellIdentityLte.pci =
        env->CallIntMethod(id, getWwanJniCache().lteIdGetPci);
    outInfo->CellInfo.lte.cellIdentityLte.tac =
        env->CallIntMethod(id, getWwanJniCache().lteIdGetTac);
    outInfo->CellInfo.lte.cellIdentityLte.earfcn = INT32_MAX;
    env->DeleteLocalRef(id);
  } else {
    outInfo->CellInfo.lte.cellIdentityLte.mcc = INT32_MAX;
  }

  if (sig) {
    outInfo->CellInfo.lte.signalStrengthLte.signalStrength =
        env->CallIntMethod(sig, getWwanJniCache().lteSigGetDbm);
    outInfo->CellInfo.lte.signalStrengthLte.rsrp =
        env->CallIntMethod(sig, getWwanJniCache().lteSigGetRsrp);
    outInfo->CellInfo.lte.signalStrengthLte.rsrq =
        env->CallIntMethod(sig, getWwanJniCache().lteSigGetRsrq);
    outInfo->CellInfo.lte.signalStrengthLte.rssnr =
        env->CallIntMethod(sig, getWwanJniCache().lteSigGetRssnr);
    outInfo->CellInfo.lte.signalStrengthLte.timingAdvance =
        env->CallIntMethod(sig, getWwanJniCache().lteSigGetTa);
    outInfo->CellInfo.lte.signalStrengthLte.cqi =
        INT32_MAX;  // Typically not available in cache
    env->DeleteLocalRef(sig);
  }
}

// WCDMA Parser
static void parseWcdmaInfo(JNIEnv *env, jobject cellInfo,
                           chreWwanCellInfo *outInfo) {
  outInfo->cellInfoType = CHRE_WWAN_CELL_INFO_TYPE_WCDMA;

  jobject id =
      env->CallObjectMethod(cellInfo, getWwanJniCache().wcdmaGetIdentity);
  jobject sig =
      env->CallObjectMethod(cellInfo, getWwanJniCache().wcdmaGetSignal);

  if (id) {
    outInfo->CellInfo.wcdma.cellIdentityWcdma.mcc =
        env->CallIntMethod(id, getWwanJniCache().wcdmaIdGetMcc);
    outInfo->CellInfo.wcdma.cellIdentityWcdma.mnc =
        env->CallIntMethod(id, getWwanJniCache().wcdmaIdGetMnc);
    outInfo->CellInfo.wcdma.cellIdentityWcdma.lac =
        env->CallIntMethod(id, getWwanJniCache().wcdmaIdGetLac);
    outInfo->CellInfo.wcdma.cellIdentityWcdma.cid =
        env->CallIntMethod(id, getWwanJniCache().wcdmaIdGetCid);
    outInfo->CellInfo.wcdma.cellIdentityWcdma.psc =
        env->CallIntMethod(id, getWwanJniCache().wcdmaIdGetPsc);
    outInfo->CellInfo.wcdma.cellIdentityWcdma.uarfcn =
        env->CallIntMethod(id, getWwanJniCache().wcdmaIdGetUarfcn);
    env->DeleteLocalRef(id);
  }

  if (sig) {
    outInfo->CellInfo.wcdma.signalStrengthWcdma.signalStrength =
        env->CallIntMethod(sig, getWwanJniCache().wcdmaSigGetDbm);
    // WCDMA getBitErrorRate is not available in standard API
    outInfo->CellInfo.wcdma.signalStrengthWcdma.bitErrorRate = INT32_MAX;
    env->DeleteLocalRef(sig);
  }
}

// NR (5G) Parser
static void parseNrInfo(JNIEnv *env, jobject cellInfo,
                        chreWwanCellInfo *outInfo) {
  outInfo->cellInfoType = CHRE_WWAN_CELL_INFO_TYPE_NR;

  if (!getWwanJniCache().cellInfoNrClass) return;  // Safety check

  jobject id = env->CallObjectMethod(cellInfo, getWwanJniCache().nrGetIdentity);
  jobject sig = env->CallObjectMethod(cellInfo, getWwanJniCache().nrGetSignal);

  if (id) {
    jstring mccStr =
        (jstring)env->CallObjectMethod(id, getWwanJniCache().nrIdGetMcc);
    jstring mncStr =
        (jstring)env->CallObjectMethod(id, getWwanJniCache().nrIdGetMnc);

    outInfo->CellInfo.nr.cellIdentityNr.mcc = parseStringToInt(env, mccStr);
    outInfo->CellInfo.nr.cellIdentityNr.mnc = parseStringToInt(env, mncStr);
    chreWwanPackNrNci(env->CallLongMethod(id, getWwanJniCache().nrIdGetNci),
                      &(outInfo->CellInfo.nr.cellIdentityNr));
    outInfo->CellInfo.nr.cellIdentityNr.pci =
        env->CallIntMethod(id, getWwanJniCache().nrIdGetPci);
    outInfo->CellInfo.nr.cellIdentityNr.tac =
        env->CallIntMethod(id, getWwanJniCache().nrIdGetTac);
    outInfo->CellInfo.nr.cellIdentityNr.nrarfcn =
        env->CallIntMethod(id, getWwanJniCache().nrIdGetNrarfcn);

    env->DeleteLocalRef(id);
  }

  if (sig) {
    outInfo->CellInfo.nr.signalStrengthNr.ssRsrp =
        env->CallIntMethod(sig, getWwanJniCache().nrSigGetSsRsrp);
    outInfo->CellInfo.nr.signalStrengthNr.ssRsrq =
        env->CallIntMethod(sig, getWwanJniCache().nrSigGetSsRsrq);
    outInfo->CellInfo.nr.signalStrengthNr.ssSinr =
        env->CallIntMethod(sig, getWwanJniCache().nrSigGetSsSinr);
    // CSI is often optional or not available in basic CellSignalStrengthNr,
    // depends on method availability
    outInfo->CellInfo.nr.signalStrengthNr.csiRsrp = INT32_MAX;
    outInfo->CellInfo.nr.signalStrengthNr.csiRsrq = INT32_MAX;
    outInfo->CellInfo.nr.signalStrengthNr.csiSinr = INT32_MAX;

    env->DeleteLocalRef(sig);
  }
}

// CDMA Parser
static void parseCdmaInfo(JNIEnv *env, jobject cellInfo,
                          chreWwanCellInfo *outInfo) {
  outInfo->cellInfoType = CHRE_WWAN_CELL_INFO_TYPE_CDMA;

  jobject id =
      env->CallObjectMethod(cellInfo, getWwanJniCache().cdmaGetIdentity);
  jobject sig =
      env->CallObjectMethod(cellInfo, getWwanJniCache().cdmaGetSignal);

  if (id) {
    outInfo->CellInfo.cdma.cellIdentityCdma.networkId =
        env->CallIntMethod(id, getWwanJniCache().cdmaIdGetNetworkId);
    outInfo->CellInfo.cdma.cellIdentityCdma.systemId =
        env->CallIntMethod(id, getWwanJniCache().cdmaIdGetSystemId);
    outInfo->CellInfo.cdma.cellIdentityCdma.basestationId =
        env->CallIntMethod(id, getWwanJniCache().cdmaIdGetBasestationId);
    outInfo->CellInfo.cdma.cellIdentityCdma.longitude =
        env->CallIntMethod(id, getWwanJniCache().cdmaIdGetLongitude);
    outInfo->CellInfo.cdma.cellIdentityCdma.latitude =
        env->CallIntMethod(id, getWwanJniCache().cdmaIdGetLatitude);
    env->DeleteLocalRef(id);
  } else {
    outInfo->CellInfo.cdma.cellIdentityCdma.networkId = INT32_MAX;
  }

  if (sig) {
    outInfo->CellInfo.cdma.signalStrengthCdma.dbm =
        env->CallIntMethod(sig, getWwanJniCache().cdmaSigGetCdmaDbm);
    outInfo->CellInfo.cdma.signalStrengthCdma.ecio =
        env->CallIntMethod(sig, getWwanJniCache().cdmaSigGetCdmaEcio);
    env->DeleteLocalRef(sig);
  }
}

// TD-SCDMA Parser
static void parseTdscdmaInfo(JNIEnv *env, jobject cellInfo,
                             chreWwanCellInfo *outInfo) {
  outInfo->cellInfoType = CHRE_WWAN_CELL_INFO_TYPE_TD_SCDMA;

  jobject id =
      env->CallObjectMethod(cellInfo, getWwanJniCache().tdscdmaGetIdentity);
  jobject sig =
      env->CallObjectMethod(cellInfo, getWwanJniCache().tdscdmaGetSignal);

  if (id) {
    jstring mccStr =
        (jstring)env->CallObjectMethod(id, getWwanJniCache().tdscdmaIdGetMcc);
    jstring mncStr =
        (jstring)env->CallObjectMethod(id, getWwanJniCache().tdscdmaIdGetMnc);
    outInfo->CellInfo.tdscdma.cellIdentityTdscdma.mcc =
        parseStringToInt(env, mccStr);
    outInfo->CellInfo.tdscdma.cellIdentityTdscdma.mnc =
        parseStringToInt(env, mncStr);

    outInfo->CellInfo.tdscdma.cellIdentityTdscdma.lac =
        env->CallIntMethod(id, getWwanJniCache().tdscdmaIdGetLac);
    outInfo->CellInfo.tdscdma.cellIdentityTdscdma.cid =
        env->CallIntMethod(id, getWwanJniCache().tdscdmaIdGetCid);
    outInfo->CellInfo.tdscdma.cellIdentityTdscdma.cpid =
        env->CallIntMethod(id, getWwanJniCache().tdscdmaIdGetCpid);
    env->DeleteLocalRef(id);
  } else {
    outInfo->CellInfo.tdscdma.cellIdentityTdscdma.mcc = INT32_MAX;
  }

  if (sig) {
    outInfo->CellInfo.tdscdma.signalStrengthTdscdma.rscp =
        env->CallIntMethod(sig, getWwanJniCache().tdscdmaSigGetDbm);
    env->DeleteLocalRef(sig);
  }
}
}  // namespace

PlatformWwan::~PlatformWwan() {}

void PlatformWwan::init() {
  JniManagerSingleton::get()->registerHandleCellInfoCallbackFn(
      [](JNIEnv *env, jobjectArray cellInfoList) {
        if (cellInfoList == nullptr) return;

        jsize count = env->GetArrayLength(cellInfoList);

        auto *result = static_cast<chreWwanCellInfoResult *>(
            memoryAlloc(sizeof(chreWwanCellInfoResult)));

        if (!result) {
          return;
        }

        result->errorCode = CHRE_ERROR_NONE;
        result->cellInfoCount = static_cast<uint8_t>(count);
        result->reserved = 0;
        result->cookie = nullptr;
        result->cells = nullptr;

        if (count > 0) {
          result->cells = static_cast<chreWwanCellInfo *>(
              memoryAlloc(sizeof(chreWwanCellInfo) * count));

          if (!result->cells) {
            memoryFree(result);
            return;
          }

          for (int i = 0; i < count; i++) {
            jobject infoObj = env->GetObjectArrayElement(cellInfoList, i);
            if (!infoObj) continue;

            chreWwanCellInfo *currCell =
                const_cast<chreWwanCellInfo *>(&result->cells[i]);

            // Populate common fields (timeStamp, registered, etc.)
            populateCommonFields(env, infoObj, currCell);

            // Dispatch based on type
            if (env->IsInstanceOf(infoObj,
                                  getWwanJniCache().cellInfoLteClass)) {
              parseLteInfo(env, infoObj, currCell);
            } else if (env->IsInstanceOf(infoObj,
                                         getWwanJniCache().cellInfoGsmClass)) {
              parseGsmInfo(env, infoObj, currCell);
            } else if (env->IsInstanceOf(
                           infoObj, getWwanJniCache().cellInfoWcdmaClass)) {
              parseWcdmaInfo(env, infoObj, currCell);
            } else if (getWwanJniCache().cellInfoNrClass &&
                       env->IsInstanceOf(infoObj,
                                         getWwanJniCache().cellInfoNrClass)) {
              parseNrInfo(env, infoObj, currCell);
            } else if (getWwanJniCache().cellInfoCdmaClass &&
                       env->IsInstanceOf(infoObj,
                                         getWwanJniCache().cellInfoCdmaClass)) {
              parseCdmaInfo(env, infoObj, currCell);
            } else if (getWwanJniCache().cellInfoTdscdmaClass &&
                       env->IsInstanceOf(
                           infoObj, getWwanJniCache().cellInfoTdscdmaClass)) {
              parseTdscdmaInfo(env, infoObj, currCell);
            } else {
              // Should not reach here and set 0 as invalid type.
              currCell->cellInfoType = 0;
            }

            env->DeleteLocalRef(infoObj);
          }
        }

        EventLoopManagerSingleton::get()
            ->getWwanRequestManager()
            .handleCellInfoResult(result);
      });
}

uint32_t PlatformWwan::getCapabilities() {
  JNIEnv *env = getEnv();
  if (!env || !getWwanJniCache().contextHubNativeClass ||
      !getWwanJniCache().getCapabilitiesMethod) {
    LOGE("JNI not ready for getCapabilities");
    return CHRE_WWAN_CAPABILITIES_NONE;
  }
  return env->CallStaticIntMethod(getWwanJniCache().contextHubNativeClass,
                                  getWwanJniCache().getCapabilitiesMethod);
}

bool PlatformWwan::requestCellInfo() {
  JNIEnv *env = getEnv();
  if (!env || !getWwanJniCache().contextHubNativeClass ||
      !getWwanJniCache().requestCellInfoMethod) {
    LOGE("JNI not ready for requestCellInfo");
    return false;
  }

  // Call Java. Java will check cache and call back native onCellInfoReceived ->
  // handleCellInfoCallback
  return env->CallStaticBooleanMethod(getWwanJniCache().contextHubNativeClass,
                                      getWwanJniCache().requestCellInfoMethod);
}

void PlatformWwan::releaseCellInfoResult(chreWwanCellInfoResult *result) {
  if (result) {
    if (result->cells) {
      memoryFree(const_cast<chreWwanCellInfo *>(result->cells));
    }
    memoryFree(result);
  }
}

}  // namespace chre

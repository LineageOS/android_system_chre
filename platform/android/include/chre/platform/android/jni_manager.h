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

#ifndef CHRE_PLATFORM_ANDROID_JNI_MANAGER_H_
#define CHRE_PLATFORM_ANDROID_JNI_MANAGER_H_

#include "chre/util/singleton.h"
#include "chre_api/chre/wwan.h"

#include "jni.h"

#include <cstdint>

namespace chre {

/**
 * A class that keeps track of the JNI related objects and methods.
 */
class JniManager {
 public:
  using HandleCellInfoCallbackFn = void (*)(JNIEnv *, jobjectArray);

  JniManager() = default;
  ~JniManager() = default;

  void init(JavaVM *vm);

  void registerHandleCellInfoCallbackFn(HandleCellInfoCallbackFn fn);

  void onCellInfoReceived(JNIEnv *env, jobjectArray cellInfoList);

  JavaVM *mJavaVm = nullptr;

  JNIEnv *getOrAttachJniEnv();

  struct WwanJniCache {
    // ContextHubAPNative class
    jclass contextHubNativeClass;
    jmethodID getCapabilitiesMethod;
    jmethodID requestCellInfoMethod;

    // Classes
    jclass cellInfoLteClass;
    jclass cellInfoGsmClass;
    jclass cellInfoWcdmaClass;
    jclass cellInfoTdscdmaClass;
    jclass cellInfoNrClass;

    // Base CellInfo methods
    jmethodID getTimeStamp;
    jmethodID isRegistered;

    // --- LTE Methods ---
    jmethodID lteGetIdentity;
    jmethodID lteGetSignal;
    // CellIdentityLte
    jmethodID lteIdGetMcc;
    jmethodID lteIdGetMnc;
    jmethodID lteIdGetCi;
    jmethodID lteIdGetPci;
    jmethodID lteIdGetTac;
    // CellSignalStrengthLte
    jmethodID lteSigGetDbm;
    jmethodID lteSigGetRsrp;
    jmethodID lteSigGetRsrq;
    jmethodID lteSigGetRssnr;
    jmethodID lteSigGetTa;

    // --- GSM Methods ---
    jmethodID gsmGetIdentity;
    jmethodID gsmGetSignal;
    // CellIdentityGsm
    jmethodID gsmIdGetMcc;
    jmethodID gsmIdGetMnc;
    jmethodID gsmIdGetLac;
    jmethodID gsmIdGetCid;
    jmethodID gsmIdGetArfcn;
    jmethodID gsmIdGetBsic;
    // CellSignalStrengthGsm
    jmethodID gsmSigGetDbm;
    jmethodID gsmSigGetBitErrorRate;

    // --- WCDMA Methods ---
    jmethodID wcdmaGetIdentity;
    jmethodID wcdmaGetSignal;
    // CellIdentityWcdma
    jmethodID wcdmaIdGetMcc;
    jmethodID wcdmaIdGetMnc;
    jmethodID wcdmaIdGetLac;
    jmethodID wcdmaIdGetCid;
    jmethodID wcdmaIdGetPsc;
    jmethodID wcdmaIdGetUarfcn;
    // CellSignalStrengthWcdma
    jmethodID wcdmaSigGetDbm;
    jmethodID wcdmaSigGetBer;

    // --- NR (5G) Methods ---
    jmethodID nrGetIdentity;
    jmethodID nrGetSignal;
    // CellIdentityNr
    jmethodID nrIdGetMcc;
    jmethodID nrIdGetMnc;
    jmethodID nrIdGetNci;
    jmethodID nrIdGetPci;
    jmethodID nrIdGetTac;
    jmethodID nrIdGetNrarfcn;
    // CellSignalStrengthNr
    jmethodID nrSigGetDbm;
    jmethodID nrSigGetCsiRsrp;
    jmethodID nrSigGetCsiRsrq;
    jmethodID nrSigGetCsiSinr;
    jmethodID nrSigGetSsRsrp;
    jmethodID nrSigGetSsRsrq;
    jmethodID nrSigGetSsSinr;

    // --- CDMA Methods ---
    jclass cellInfoCdmaClass;
    jmethodID cdmaGetIdentity;
    jmethodID cdmaGetSignal;
    // CellIdentityCdma
    jmethodID cdmaIdGetNetworkId;
    jmethodID cdmaIdGetSystemId;
    jmethodID cdmaIdGetBasestationId;
    jmethodID cdmaIdGetLongitude;
    jmethodID cdmaIdGetLatitude;
    // CellSignalStrengthCdma
    jmethodID cdmaSigGetCdmaDbm;
    jmethodID cdmaSigGetCdmaEcio;

    // --- TD-SCDMA Methods ---
    jmethodID tdscdmaGetIdentity;
    jmethodID tdscdmaGetSignal;
    // CellIdentityTdscdma
    jmethodID tdscdmaIdGetMcc;
    jmethodID tdscdmaIdGetMnc;
    jmethodID tdscdmaIdGetLac;
    jmethodID tdscdmaIdGetCid;
    jmethodID tdscdmaIdGetCpid;
    jmethodID tdscdmaSigGetDbm;
    // CellSignalStrengthTdscdma
    jmethodID tdscdmaSigGetRscp;
  };
  WwanJniCache mWwanJniCache;

  HandleCellInfoCallbackFn mHandleCellInfoCallbackFn;
};

//! Provide an alias to the JniManager singleton.
typedef Singleton<JniManager> JniManagerSingleton;

}  // namespace chre

#endif  // CHRE_PLATFORM_ANDROID_JNI_MANAGER_H_
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

#include "chre/platform/android/jni_manager.h"
#include "chre/platform/log.h"

#include "jni.h"

namespace chre {

void JniManager::init(JavaVM *vm) {
  mJavaVm = vm;
  if (vm == nullptr) {
    LOGE("JniManager init failed: JavaVM is null");
    return;
  }

  JNIEnv *env = nullptr;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    LOGE("JniManager init failed: Could not get JNIEnv from JavaVM");
    return;
  }

  // --- WWAN Initialization ---
  // ContextHubAPNative
  jclass nativeCls =
      env->FindClass("com/google/android/chre/ap/ContextHubAPNative");
  if (nativeCls) {
    mWwanJniCache.contextHubNativeClass = (jclass)env->NewGlobalRef(nativeCls);
    mWwanJniCache.getCapabilitiesMethod =
        env->GetStaticMethodID(nativeCls, "getWwanCapabilities", "()I");
    mWwanJniCache.requestCellInfoMethod =
        env->GetStaticMethodID(nativeCls, "requestWwanCellInfo", "()Z");
  }

  // Base CellInfo
  jclass baseCi = env->FindClass("android/telephony/CellInfo");
  if (baseCi) {
    mWwanJniCache.getTimeStamp =
        env->GetMethodID(baseCi, "getTimeStamp", "()J");
    mWwanJniCache.isRegistered =
        env->GetMethodID(baseCi, "isRegistered", "()Z");
  }

  // LTE
  jclass ciLte = env->FindClass("android/telephony/CellInfoLte");
  jclass idLte = env->FindClass("android/telephony/CellIdentityLte");
  jclass sigLte = env->FindClass("android/telephony/CellSignalStrengthLte");
  if (ciLte && idLte && sigLte) {
    mWwanJniCache.cellInfoLteClass = (jclass)env->NewGlobalRef(ciLte);

    mWwanJniCache.lteGetIdentity = env->GetMethodID(
        ciLte, "getCellIdentity", "()Landroid/telephony/CellIdentityLte;");
    mWwanJniCache.lteGetSignal =
        env->GetMethodID(ciLte, "getCellSignalStrength",
                         "()Landroid/telephony/CellSignalStrengthLte;");

    mWwanJniCache.lteIdGetMcc = env->GetMethodID(idLte, "getMcc", "()I");
    mWwanJniCache.lteIdGetMnc = env->GetMethodID(idLte, "getMnc", "()I");
    mWwanJniCache.lteIdGetCi = env->GetMethodID(idLte, "getCi", "()I");
    mWwanJniCache.lteIdGetPci = env->GetMethodID(idLte, "getPci", "()I");
    mWwanJniCache.lteIdGetTac = env->GetMethodID(idLte, "getTac", "()I");

    mWwanJniCache.lteSigGetDbm = env->GetMethodID(sigLte, "getDbm", "()I");
    mWwanJniCache.lteSigGetRsrp = env->GetMethodID(sigLte, "getRsrp", "()I");
    mWwanJniCache.lteSigGetRsrq = env->GetMethodID(sigLte, "getRsrq", "()I");
    mWwanJniCache.lteSigGetRssnr = env->GetMethodID(sigLte, "getRssnr", "()I");
    mWwanJniCache.lteSigGetTa =
        env->GetMethodID(sigLte, "getTimingAdvance", "()I");
  }

  // GSM
  jclass ciGsm = env->FindClass("android/telephony/CellInfoGsm");
  jclass idGsm = env->FindClass("android/telephony/CellIdentityGsm");
  jclass sigGsm = env->FindClass("android/telephony/CellSignalStrengthGsm");
  if (ciGsm && idGsm && sigGsm) {
    mWwanJniCache.cellInfoGsmClass = (jclass)env->NewGlobalRef(ciGsm);
    mWwanJniCache.gsmGetIdentity = env->GetMethodID(
        ciGsm, "getCellIdentity", "()Landroid/telephony/CellIdentityGsm;");
    mWwanJniCache.gsmGetSignal =
        env->GetMethodID(ciGsm, "getCellSignalStrength",
                         "()Landroid/telephony/CellSignalStrengthGsm;");

    mWwanJniCache.gsmIdGetMcc = env->GetMethodID(idGsm, "getMcc", "()I");
    mWwanJniCache.gsmIdGetMnc = env->GetMethodID(idGsm, "getMnc", "()I");
    mWwanJniCache.gsmIdGetLac = env->GetMethodID(idGsm, "getLac", "()I");
    mWwanJniCache.gsmIdGetCid = env->GetMethodID(idGsm, "getCid", "()I");
    mWwanJniCache.gsmIdGetArfcn = env->GetMethodID(idGsm, "getArfcn", "()I");
    mWwanJniCache.gsmIdGetBsic = env->GetMethodID(idGsm, "getBsic", "()I");

    mWwanJniCache.gsmSigGetDbm = env->GetMethodID(sigGsm, "getDbm", "()I");
    mWwanJniCache.gsmSigGetBitErrorRate =
        env->GetMethodID(sigGsm, "getBitErrorRate", "()I");
  }

  // WCDMA
  jclass ciWcdma = env->FindClass("android/telephony/CellInfoWcdma");
  jclass idWcdma = env->FindClass("android/telephony/CellIdentityWcdma");
  jclass sigWcdma = env->FindClass("android/telephony/CellSignalStrengthWcdma");
  if (ciWcdma && idWcdma && sigWcdma) {
    mWwanJniCache.cellInfoWcdmaClass = (jclass)env->NewGlobalRef(ciWcdma);
    mWwanJniCache.wcdmaGetIdentity = env->GetMethodID(
        ciWcdma, "getCellIdentity", "()Landroid/telephony/CellIdentityWcdma;");
    mWwanJniCache.wcdmaGetSignal =
        env->GetMethodID(ciWcdma, "getCellSignalStrength",
                         "()Landroid/telephony/CellSignalStrengthWcdma;");

    mWwanJniCache.wcdmaIdGetMcc = env->GetMethodID(idWcdma, "getMcc", "()I");
    mWwanJniCache.wcdmaIdGetMnc = env->GetMethodID(idWcdma, "getMnc", "()I");
    mWwanJniCache.wcdmaIdGetLac = env->GetMethodID(idWcdma, "getLac", "()I");
    mWwanJniCache.wcdmaIdGetCid = env->GetMethodID(idWcdma, "getCid", "()I");
    mWwanJniCache.wcdmaIdGetPsc = env->GetMethodID(idWcdma, "getPsc", "()I");
    mWwanJniCache.wcdmaIdGetUarfcn =
        env->GetMethodID(idWcdma, "getUarfcn", "()I");

    mWwanJniCache.wcdmaSigGetDbm = env->GetMethodID(sigWcdma, "getDbm", "()I");
  }

  // NR (5G)
  jclass ciNr = env->FindClass("android/telephony/CellInfoNr");
  jclass idNr = env->FindClass("android/telephony/CellIdentityNr");
  jclass sigNr = env->FindClass("android/telephony/CellSignalStrengthNr");
  if (ciNr && idNr && sigNr) {
    mWwanJniCache.cellInfoNrClass = (jclass)env->NewGlobalRef(ciNr);
    mWwanJniCache.nrGetIdentity = env->GetMethodID(
        ciNr, "getCellIdentity", "()Landroid/telephony/CellIdentity;");
    mWwanJniCache.nrGetSignal =
        env->GetMethodID(ciNr, "getCellSignalStrength",
                         "()Landroid/telephony/CellSignalStrength;");

    mWwanJniCache.nrIdGetMcc =
        env->GetMethodID(idNr, "getMccString", "()Ljava/lang/String;");
    mWwanJniCache.nrIdGetMnc =
        env->GetMethodID(idNr, "getMncString", "()Ljava/lang/String;");
    mWwanJniCache.nrIdGetNci = env->GetMethodID(idNr, "getNci", "()J");
    mWwanJniCache.nrIdGetPci = env->GetMethodID(idNr, "getPci", "()I");
    mWwanJniCache.nrIdGetTac = env->GetMethodID(idNr, "getTac", "()I");
    mWwanJniCache.nrIdGetNrarfcn = env->GetMethodID(idNr, "getNrarfcn", "()I");

    mWwanJniCache.nrSigGetDbm = env->GetMethodID(sigNr, "getDbm", "()I");
    mWwanJniCache.nrSigGetSsRsrp = env->GetMethodID(sigNr, "getSsRsrp", "()I");
    mWwanJniCache.nrSigGetSsRsrq = env->GetMethodID(sigNr, "getSsRsrq", "()I");
    mWwanJniCache.nrSigGetSsSinr = env->GetMethodID(sigNr, "getSsSinr", "()I");
  }

  // CDMA
  jclass ciCdma = env->FindClass("android/telephony/CellInfoCdma");
  jclass idCdma = env->FindClass("android/telephony/CellIdentityCdma");
  jclass sigCdma = env->FindClass("android/telephony/CellSignalStrengthCdma");
  if (ciCdma && idCdma && sigCdma) {
    mWwanJniCache.cellInfoCdmaClass = (jclass)env->NewGlobalRef(ciCdma);
    mWwanJniCache.cdmaGetIdentity = env->GetMethodID(
        ciCdma, "getCellIdentity", "()Landroid/telephony/CellIdentityCdma;");
    mWwanJniCache.cdmaGetSignal =
        env->GetMethodID(ciCdma, "getCellSignalStrength",
                         "()Landroid/telephony/CellSignalStrengthCdma;");

    mWwanJniCache.cdmaIdGetNetworkId =
        env->GetMethodID(idCdma, "getNetworkId", "()I");
    mWwanJniCache.cdmaIdGetSystemId =
        env->GetMethodID(idCdma, "getSystemId", "()I");
    mWwanJniCache.cdmaIdGetBasestationId =
        env->GetMethodID(idCdma, "getBasestationId", "()I");
    mWwanJniCache.cdmaIdGetLongitude =
        env->GetMethodID(idCdma, "getLongitude", "()I");
    mWwanJniCache.cdmaIdGetLatitude =
        env->GetMethodID(idCdma, "getLatitude", "()I");

    mWwanJniCache.cdmaSigGetCdmaDbm =
        env->GetMethodID(sigCdma, "getCdmaDbm", "()I");
    mWwanJniCache.cdmaSigGetCdmaEcio =
        env->GetMethodID(sigCdma, "getCdmaEcio", "()I");
  }

  // TD-SCDMA
  jclass ciTdscdma = env->FindClass("android/telephony/CellInfoTdscdma");
  jclass idTdscdma = env->FindClass("android/telephony/CellIdentityTdscdma");
  jclass sigTdscdma =
      env->FindClass("android/telephony/CellSignalStrengthTdscdma");
  if (ciTdscdma && idTdscdma && sigTdscdma) {
    mWwanJniCache.cellInfoTdscdmaClass = (jclass)env->NewGlobalRef(ciTdscdma);
    mWwanJniCache.tdscdmaGetIdentity =
        env->GetMethodID(ciTdscdma, "getCellIdentity",
                         "()Landroid/telephony/CellIdentityTdscdma;");
    mWwanJniCache.tdscdmaGetSignal =
        env->GetMethodID(ciTdscdma, "getCellSignalStrength",
                         "()Landroid/telephony/CellSignalStrengthTdscdma;");

    mWwanJniCache.tdscdmaIdGetMcc =
        env->GetMethodID(idTdscdma, "getMccString", "()Ljava/lang/String;");
    mWwanJniCache.tdscdmaIdGetMnc =
        env->GetMethodID(idTdscdma, "getMncString", "()Ljava/lang/String;");
    mWwanJniCache.tdscdmaIdGetLac =
        env->GetMethodID(idTdscdma, "getLac", "()I");
    mWwanJniCache.tdscdmaIdGetCid =
        env->GetMethodID(idTdscdma, "getCid", "()I");
    mWwanJniCache.tdscdmaIdGetCpid =
        env->GetMethodID(idTdscdma, "getCpid", "()I");

    mWwanJniCache.tdscdmaSigGetDbm =
        env->GetMethodID(sigTdscdma, "getDbm", "()I");
  }
}

void JniManager::registerHandleCellInfoCallbackFn(HandleCellInfoCallbackFn fn) {
  mHandleCellInfoCallbackFn = fn;
}

void JniManager::onCellInfoReceived(JNIEnv *env, jobjectArray cellInfoList) {
  if (mHandleCellInfoCallbackFn != nullptr) {
    mHandleCellInfoCallbackFn(env, cellInfoList);
  } else {
    LOGW("CellInfo received but no callback registered in JniManager");
  }
}

JNIEnv *JniManager::getOrAttachJniEnv() {
  if (mJavaVm == nullptr) {
    LOGE("JavaVM not initialized");
    return nullptr;
  }

  JNIEnv *env = nullptr;
  jint res = mJavaVm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

  if (res == JNI_OK) {
    return env;
  } else if (res == JNI_EDETACHED) {
    if (mJavaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
      LOGE("Failed to attach thread to JavaVM");
      return nullptr;
    }
    return env;
  } else {
    LOGE("Failed to get JNIEnv: %d", res);
    return nullptr;
  }
}

}  // namespace chre

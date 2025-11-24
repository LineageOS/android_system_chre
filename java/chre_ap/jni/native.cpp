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
#define LOG_TAG "CHRE_AP"

#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/static_nanoapps.h"
#include "chre/platform/android/jni_manager.h"
#include "chre/platform/android/platform_log.h"
#include "chre/platform/shared/init.h"
#include "chre/platform/system_timer.h"
#include "chrex_api/chrex_wake_lock.h"

#include <android/log_macros.h>

#include <cstdio>
#include <memory>
#include <thread>

#include "jni.h"

// Global thread to run CHRE event loop.
std::unique_ptr<std::thread> chreThread = nullptr;
bool g_chre_initialized = false;

static JavaVM *g_javaVM = nullptr;
static jobject g_contextHubAPManagerInstance = nullptr;
static jmethodID g_onMessageMethodID = nullptr;
static jclass g_alarmBridgeClass = nullptr;
static jmethodID g_setAlarmMethod = nullptr;
static jmethodID g_cancelAlarmMethod = nullptr;
static jclass g_wakeLockBridgeClass = nullptr;
static jmethodID g_wakeLockAcquireMethod = nullptr;
static jmethodID g_wakeLockReleaseMethod = nullptr;

void messageCallback(int64_t nanoAppId, int32_t messageType, void *messageBody,
                     size_t messageBodyLen) {
  ALOGD("Received nanoapp message, nanoAppid: %" PRId64
        ", messageType: %" PRId32 ", message: %p, length: %zu",
        nanoAppId, messageType, messageBody, messageBodyLen);

  if (!g_javaVM || !g_contextHubAPManagerInstance || !g_onMessageMethodID) {
    ALOGE("JNI environment not ready to call Java method.");
    return;
  }

  JNIEnv *env;
  jint attachResult = g_javaVM->AttachCurrentThread(&env, nullptr);
  if (attachResult != JNI_OK) {
    ALOGE("Failed to attach current thread to JVM.");
    return;
  }

  jbyteArray messagePayload = env->NewByteArray(messageBodyLen);
  env->SetByteArrayRegion(messagePayload, 0, messageBodyLen,
                          reinterpret_cast<const jbyte *>(messageBody));

  env->CallVoidMethod(g_contextHubAPManagerInstance, g_onMessageMethodID,
                      static_cast<jlong>(nanoAppId),
                      static_cast<jint>(messageType), messagePayload);
  if (env->ExceptionCheck()) {
    ALOGE("An exception occurred during the call to onMessageFromNanoApp.");
    env->ExceptionDescribe();
    env->ExceptionClear();
  }

  env->DeleteLocalRef(messagePayload);

  if (chreThread) {
    g_javaVM->DetachCurrentThread();
  }
}

void chreApSetAlarm(uint64_t timerId, uint64_t delayNs) {
  if (g_javaVM == nullptr || g_alarmBridgeClass == nullptr ||
      g_setAlarmMethod == nullptr) {
    ALOGE("JNI not initialized for setAlarm");
    return;
  }

  JNIEnv *env;
  jint attachResult = g_javaVM->AttachCurrentThread(&env, nullptr);
  if (attachResult != JNI_OK) {
    ALOGE("Failed to attach current thread to JVM.");
    return;
  }

  uint64_t delayMs = delayNs / 1000000;
  env->CallStaticVoidMethod(g_alarmBridgeClass, g_setAlarmMethod,
                            (jlong)timerId, (jlong)delayMs);

  if (chreThread) {
    g_javaVM->DetachCurrentThread();
  }
}

void chreApCancelAlarm(uint64_t timerId) {
  if (g_javaVM == nullptr || g_alarmBridgeClass == nullptr ||
      g_setAlarmMethod == nullptr) {
    ALOGE("JNI not initialized for setAlarm");
    return;
  }

  JNIEnv *env;
  jint attachResult = g_javaVM->AttachCurrentThread(&env, nullptr);
  if (attachResult != JNI_OK) {
    ALOGE("Failed to attach current thread to JVM.");
    return;
  }

  env->CallStaticVoidMethod(g_alarmBridgeClass, g_cancelAlarmMethod,
                            (jlong)timerId);

  if (chreThread) {
    g_javaVM->DetachCurrentThread();
  }
}

void onAlarmFired(JNIEnv * /*env*/, jclass /*clazz*/, jlong timerId) {
  chre::SystemTimerBase::systemTimerNotifyCallback(
      {.sival_ptr = (void *)timerId});
}

void chrexApWakeLockAcquire(const std::string &lock_name,
                            uint64_t timeout_millis) {
  if (g_javaVM == nullptr || g_wakeLockBridgeClass == nullptr ||
      g_wakeLockAcquireMethod == nullptr) {
    ALOGE("JNI not initialized for wake lock acquire");
    return;
  }

  JNIEnv *env;
  jint attachResult = g_javaVM->AttachCurrentThread(&env, nullptr);
  if (attachResult != JNI_OK) {
    ALOGE("Failed to attach current thread to JVM.");
    return;
  }
  jstring nameStr = env->NewStringUTF(lock_name.c_str());
  env->CallStaticVoidMethod(g_wakeLockBridgeClass, g_wakeLockAcquireMethod,
                            nameStr, (jlong)timeout_millis);

  env->DeleteLocalRef(nameStr);
  if (chreThread) {
    g_javaVM->DetachCurrentThread();
  }
}

void chrexApWakeLockRelease(const std::string &lock_name) {
  if (g_javaVM == nullptr || g_wakeLockBridgeClass == nullptr ||
      g_wakeLockAcquireMethod == nullptr) {
    ALOGE("JNI not initialized for wake lock acquire");
    return;
  }

  JNIEnv *env;
  jint attachResult = g_javaVM->AttachCurrentThread(&env, nullptr);
  if (attachResult != JNI_OK) {
    ALOGE("Failed to attach current thread to JVM.");
    return;
  }
  jstring nameStr = env->NewStringUTF(lock_name.c_str());
  env->CallStaticVoidMethod(g_wakeLockBridgeClass, g_wakeLockReleaseMethod,
                            nameStr);

  env->DeleteLocalRef(nameStr);
  if (chreThread) {
    g_javaVM->DetachCurrentThread();
  }
}

static void onCellInfoReceived(JNIEnv *env, jclass /*clazz*/,
                               jobjectArray cellInfoList) {
  chre::JniManagerSingleton::get()->onCellInfoReceived(env, cellInfoList);
}

static jint init(JNIEnv * /*env*/, jobject /*thiz*/) {
  if (g_chre_initialized) {
    ALOGE("Environment already initialized");
    return 0;
  }
  // Initialize the system.
  chre::JniManagerSingleton::get()->init(g_javaVM);
  chre::initCommon();
  chre::EventLoopManagerSingleton::get()->lateInit();
  chre::EventLoopManagerSingleton::get()
      ->getHostCommsManager()
      .registerMessageCallback(messageCallback);
  ALOGD("message callback function registered.");
  g_chre_initialized = true;
  return 0;
}

static void run() {
  // Load static nanoapps unless they are disabled by a command-line flag.
  chre::loadStaticNanoapps();
  ALOGD(
      "%zu nanoapps loaded",
      chre::EventLoopManagerSingleton::get()->getEventLoop().getNanoappCount());
  chre::EventLoopManagerSingleton::get()->getEventLoop().run();
}

static void runEventLoop(JNIEnv * /*env*/, jobject /*thiz*/,
                         jboolean useNativeThread) {
  if (!g_chre_initialized) {
    ALOGE("Environment not initialized");
    return;
  }
  if (useNativeThread) {
    chreThread = std::make_unique<std::thread>(run);
  } else {
    run();
  }
}

static void stopEventLoop(JNIEnv * /*env*/, jobject /*thiz*/) {
  if (!g_chre_initialized) {
    ALOGE("Environment not initialized");
    return;
  }
  chre::EventLoopManagerSingleton::get()->getEventLoop().stop();
  if (chreThread != nullptr) {
    chreThread->join();
    chreThread.reset();
  }
}

static void destroy(JNIEnv * /*env*/, jobject /*thiz*/) {
  if (!g_chre_initialized) {
    ALOGE("Environment not initialized");
    return;
  }
  chre::deinitCommon();
  g_chre_initialized = false;
  ALOGD("Environment destroyed");
}

static jint loadNanoAppFromFile(JNIEnv *env, jobject /*thiz*/,
                                jstring filename) {
  auto nanoapp = chre::MakeUnique<chre::Nanoapp>();
  nanoapp->loadFromFile(env->GetStringUTFChars(filename, nullptr));
  return chre::EventLoopManagerSingleton::get()->getEventLoop().startNanoapp(
      std::move(nanoapp));
}

static jboolean unloadNanoApp(JNIEnv * /*env*/, jobject /*thiz*/,
                              jlong nanoAppInstanceId) {
  return chre::EventLoopManagerSingleton::get()->getEventLoop().unloadNanoapp(
      nanoAppInstanceId, false /*allowSystemNanoappUnload*/);
}

static jboolean sendMessage(JNIEnv *env, jobject /*thiz*/, jlong nanoAppId,
                            jint messageType, jbyteArray message,
                            jint messageSize) {
  jbyte *javaBytes = env->GetByteArrayElements(message, nullptr);

  auto &comms_manager =
      chre::EventLoopManagerSingleton::get()->getHostCommsManager();
  comms_manager.sendMessageToNanoappFromHost(
      nanoAppId, messageType, 0, javaBytes, messageSize, false /*isReliable=*/,
      0 /*messageSequenceNumber*/);
  return true;
}

static jobjectArray listNanoapps(JNIEnv *env, jclass /*clazz*/) {
  std::vector<std::pair<uint32_t, const char *>> nanoappInfoList;
  auto nanoappCallback = [](const chre::Nanoapp *app, void *data) {
    auto *list =
        static_cast<std::vector<std::pair<uint32_t, const char *>> *>(data);
    list->push_back({app->getInstanceId(), app->getAppName()});
  };
  chre::EventLoopManagerSingleton::get()->getEventLoop().forEachNanoapp(
      nanoappCallback, &nanoappInfoList);

  const char *nanoAppInfoClassName = "com/google/android/chre/ap/NanoAppState";
  jclass nanoAppInfoClass = env->FindClass(nanoAppInfoClassName);
  if (nanoAppInfoClass == nullptr) {
    ALOGE("Failed to find class '%s'", nanoAppInfoClassName);
    return nullptr;
  }

  jmethodID constructor = env->GetMethodID(nanoAppInfoClass, "<init>", "()V");
  jfieldID instanceIdField =
      env->GetFieldID(nanoAppInfoClass, "mNanoAppId", "J");
  jfieldID nameField =
      env->GetFieldID(nanoAppInfoClass, "mName", "Ljava/lang/String;");

  if (constructor == nullptr || instanceIdField == nullptr ||
      nameField == nullptr) {
    ALOGE("Failed to find constructor or fields for NanoAppInfo");
    return nullptr;
  }

  jobjectArray nanoAppArray =
      env->NewObjectArray(nanoappInfoList.size(), nanoAppInfoClass, nullptr);
  if (nanoAppArray == nullptr) {
    ALOGE("Failed to create NanoAppInfo array");
    return nullptr;
  }

  for (size_t i = 0; i < nanoappInfoList.size(); ++i) {
    const auto &info = nanoappInfoList[i];

    jobject nanoAppInfoObj = env->NewObject(nanoAppInfoClass, constructor);
    env->SetLongField(nanoAppInfoObj, instanceIdField,
                      static_cast<jlong>(info.first));

    jstring nameStr = env->NewStringUTF(info.second);
    env->SetObjectField(nanoAppInfoObj, nameField, nameStr);

    env->SetObjectArrayElement(nanoAppArray, i, nanoAppInfoObj);

    // Clean up local references created in the loop
    env->DeleteLocalRef(nanoAppInfoObj);
    env->DeleteLocalRef(nameStr);
  }

  return nanoAppArray;
}

static void nativeRegister(JNIEnv *env, jobject, jobject instance) {
  if (g_contextHubAPManagerInstance != nullptr) {
    env->DeleteGlobalRef(g_contextHubAPManagerInstance);
  }
  g_contextHubAPManagerInstance = env->NewGlobalRef(instance);
  if (g_contextHubAPManagerInstance == nullptr) {
    ALOGE("Failed to create global reference for ContextHubAPManager.");
    return;
  }

  jclass managerClass = env->GetObjectClass(instance);
  if (managerClass == nullptr) {
    ALOGE("Failed to find class for ContextHubAPManager.");
    return;
  }
  g_onMessageMethodID =
      env->GetMethodID(managerClass, "onMessageFromNanoApp", "(JI[B)V");
  if (g_onMessageMethodID == nullptr) {
    ALOGE("Failed to find method 'onMessageFromNanoApp'.");
  } else {
    ALOGI("Successfully cached ContextHubAPManager instance and method ID.");
  }
  env->DeleteLocalRef(managerClass);

  jclass alarmBridgeClass =
      env->FindClass("com/google/android/chre/ap/AlarmManagerBridge");
  if (alarmBridgeClass == nullptr) {
    ALOGE("Failed to find class 'AlarmManagerBridge'.");
    return;
  }
  g_alarmBridgeClass = (jclass)env->NewGlobalRef(alarmBridgeClass);
  g_setAlarmMethod =
      env->GetStaticMethodID(g_alarmBridgeClass, "setAlarm", "(JJ)V");
  g_cancelAlarmMethod =
      env->GetStaticMethodID(g_alarmBridgeClass, "cancelAlarm", "(J)V");

  if (g_setAlarmMethod == nullptr || g_cancelAlarmMethod == nullptr) {
    ALOGE("Failed to find methods 'setAlarm' or 'cancelAlarm'.");
  }

  jclass wakeLockBridgeClass =
      env->FindClass("com/google/android/chre/ap/WakeLockBridge");
  if (wakeLockBridgeClass == nullptr) {
    ALOGE("Failed to find class 'WakeLockBridge'.");
    return;
  }
  g_wakeLockBridgeClass = (jclass)env->NewGlobalRef(wakeLockBridgeClass);
  g_wakeLockAcquireMethod = env->GetStaticMethodID(
      g_wakeLockBridgeClass, "acquireWakeLock", "(Ljava/lang/String;J)V");
  g_wakeLockReleaseMethod = env->GetStaticMethodID(
      g_wakeLockBridgeClass, "releaseWakeLock", "(Ljava/lang/String;)V");

  if (g_wakeLockAcquireMethod == nullptr ||
      g_wakeLockReleaseMethod == nullptr) {
    ALOGE("Cannot find methods in WakeLockBridge");
  }
}

static jboolean isInitialized(JNIEnv * /*env*/, jobject /*thiz*/) {
  return g_chre_initialized;
}

static JNINativeMethod methods[] = {
    {(char *)"init", (char *)"()I", (void *)init},
    {(char *)"destroy", (char *)"()V", (void *)destroy},
    {(char *)"runEventLoop", (char *)"(Z)V", (void *)runEventLoop},
    {(char *)"loadNanoAppFromFile", (char *)"(Ljava/lang/String;)Z",
     (void *)loadNanoAppFromFile},
    {(char *)"unloadNanoApp", (char *)"(J)Z", (void *)unloadNanoApp},
    {(char *)"listNanoapps",
     (char *)"()[Lcom/google/android/chre/ap/NanoAppState;",
     (void *)listNanoapps},
    {(char *)"sendMessage", (char *)"(JI[BI)Z", (void *)sendMessage},
    {(char *)"nativeRegister",
     (char *)"(Lcom/google/android/chre/ap/ContextHubAPManager;)V",
     (void *)nativeRegister},
    {(char *)"isInitialized", (char *)"()Z", (void *)isInitialized},
    {(char *)"stopEventLoop", (char *)"()V", (void *)stopEventLoop},
    {(char *)"onAlarmFired", (char *)"(J)V", (void *)onAlarmFired},
    {(char *)"onCellInfoReceived", (char *)"([Ljava/lang/Object;)V",
     (void *)onCellInfoReceived},
};

// Register native methods for all classes we know about.
static const char *classPathName =
    "com/google/android/chre/ap/ContextHubAPNative";

static int registerNatives(JNIEnv *env) {
  auto clazz = env->FindClass(classPathName);
  if (clazz == nullptr) {
    ALOGE("Native registration unable to find class '%s'", classPathName);
    return JNI_FALSE;
  }
  if (env->RegisterNatives(clazz, methods,
                           sizeof(methods) / sizeof(methods[0])) < 0) {
    ALOGE("RegisterNatives failed for '%s'", classPathName);
    return JNI_FALSE;
  }
  return JNI_TRUE;
}

typedef union {
  JNIEnv *env;
  void *venv;
} UnionJNIEnvToVoid;

// Called by the VM when the shared library is first loaded.
jint JNI_OnLoad(JavaVM *vm, void * /*reserved*/) {
  g_javaVM = vm;
  UnionJNIEnvToVoid uenv;
  uenv.venv = nullptr;
  JNIEnv *env = nullptr;

  ALOGI("JNI_OnLoad");

  if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_6) != JNI_OK) {
    ALOGE("ERROR: GetEnv failed");
    return -1;
  }
  env = uenv.env;

  if (registerNatives(env) != JNI_TRUE) {
    ALOGE("ERROR: registerNatives failed");
    return -2;
  }

  return JNI_VERSION_1_6;
}

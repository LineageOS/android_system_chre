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

#pragma once

/**
 * Annotations which can be used for thread-safety checks.
 *
 * Full documentation of clang thread-safety can be found here
 * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
 */

#if defined(__clang__) && (!defined(SWIG))
#define CHRE_THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define CHRE_THREAD_ANNOTATION_ATTRIBUTE__(x)  // no-op
#endif

#define CHRE_CAPABILITY(x) CHRE_THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define CHRE_REENTRANT_CAPABILITY \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(reentrant_capability)

#define CHRE_SCOPED_CAPABILITY \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define CHRE_GUARDED_BY(x) CHRE_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define CHRE_PT_GUARDED_BY(x) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define CHRE_ACQUIRED_BEFORE(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define CHRE_ACQUIRED_AFTER(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define CHRE_REQUIRES(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define CHRE_REQUIRES_SHARED(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

#define CHRE_ACQUIRE(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define CHRE_ACQUIRE_SHARED(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define CHRE_RELEASE(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define CHRE_RELEASE_SHARED(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define CHRE_RELEASE_GENERIC(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(release_generic_capability(__VA_ARGS__))

#define CHRE_TRY_ACQUIRE(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

#define CHRE_TRY_ACQUIRE_SHARED(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define CHRE_EXCLUDES(...) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define CHRE_ASSERT_CAPABILITY(x) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define CHRE_ASSERT_SHARED_CAPABILITY(x) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define CHRE_RETURN_CAPABILITY(x) \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define CHRE_NO_THREAD_SAFETY_ANALYSIS \
  CHRE_THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)
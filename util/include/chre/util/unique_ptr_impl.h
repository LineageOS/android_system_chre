/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef CHRE_UTIL_UNIQUE_PTR_IMPL_H_
#define CHRE_UTIL_UNIQUE_PTR_IMPL_H_

// IWYU pragma: private
#include "chre/util/unique_ptr.h"

#include <string.h>
#include <type_traits>
#include <utility>

#include "chre/util/container_support.h"
#include "chre/util/memory.h"

namespace chre {

template <typename ObjectOrArrayType, typename AllocatorProviderT>
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::UniquePtr()
    : mObject(nullptr) {}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::UniquePtr(
    typename UniquePtr<ObjectOrArrayType, AllocatorProviderT>::ObjectType
        *object)
    : mObject(object) {}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::UniquePtr(
    UniquePtr<ObjectOrArrayType, AllocatorProviderT> &&other) {
  mObject = other.mObject;
  other.mObject = nullptr;
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
template <typename OtherObjectOrArrayType>
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::UniquePtr(
    UniquePtr<OtherObjectOrArrayType, AllocatorProviderT> &&other) {
  static_assert(std::is_array_v<ObjectOrArrayType> ==
                    std::is_array_v<OtherObjectOrArrayType>,
                "UniquePtr conversion not supported across array and non-array "
                "types");
  mObject = other.mObject;
  other.mObject = nullptr;
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::~UniquePtr() {
  reset();
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
template <typename... Args>
void UniquePtr<ObjectOrArrayType, AllocatorProviderT>::emplace(Args &&...args) {
  reset();
  mObject = static_cast<ObjectOrArrayType *>(
      AllocatorProviderT::template allocate<ObjectType>());
  if (mObject != nullptr) {
    new (mObject) ObjectType(std::forward<Args>(args)...);
  }
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
void UniquePtr<ObjectOrArrayType, AllocatorProviderT>::makeArray(size_t count) {
  static_assert(util::internal::is_unbounded_array_v<ObjectOrArrayType>,
                "Creating an UniquePtr for an array is only supported for "
                "unbounded array types, e.g. int[]");

  reset();
  mObject = static_cast<ObjectType *>(
      AllocatorProviderT::template allocateArray<ObjectType>(count));
  if (mObject != nullptr) {
    // Note that we check in the definition of UniquePtr that if the type is an
    // array, it is trivially destructible.
    new (mObject) ObjectType[count];
  }
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
void UniquePtr<ObjectOrArrayType, AllocatorProviderT>::makeZeroFill() {
  static_assert(std::is_trivial<ObjectType>::value,
                "UniquePtr::makeZeroFill is only supported for trivial types");
  reset();
  mObject = static_cast<ObjectType *>(
      AllocatorProviderT::template allocate<ObjectType>());
  if (mObject != nullptr) {
    memset(mObject, 0, sizeof(ObjectType));
  }
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
bool UniquePtr<ObjectOrArrayType, AllocatorProviderT>::isNull() const {
  return (mObject == nullptr);
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
typename UniquePtr<ObjectOrArrayType, AllocatorProviderT>::ObjectType *
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::get() const {
  return mObject;
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
typename UniquePtr<ObjectOrArrayType, AllocatorProviderT>::ObjectType *
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::release() {
  ObjectType *obj = mObject;
  mObject = nullptr;
  return obj;
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
void UniquePtr<ObjectOrArrayType, AllocatorProviderT>::reset(
    ObjectType *object) {
  CHRE_ASSERT(object == nullptr || mObject != object);

  reset();
  mObject = object;
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
void UniquePtr<ObjectOrArrayType, AllocatorProviderT>::reset() {
  if (mObject != nullptr) {
    if constexpr (!std::is_trivially_destructible_v<ObjectType>) {
      mObject->~ObjectType();
    }
    AllocatorProviderT::deallocate(mObject);
    mObject = nullptr;
  }
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
typename UniquePtr<ObjectOrArrayType, AllocatorProviderT>::ObjectType *
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::operator->() const {
  static_assert(!std::is_array_v<ObjectOrArrayType>,
                "UniquePtr<T>::operator-> is not supported for array types");
  return get();
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
typename UniquePtr<ObjectOrArrayType, AllocatorProviderT>::ObjectType &
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::operator*() const {
  static_assert(!std::is_array_v<ObjectOrArrayType>,
                "UniquePtr<T>::operator* is not supported for array types");
  return *get();
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
typename UniquePtr<ObjectOrArrayType, AllocatorProviderT>::ObjectType &
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::operator[](
    size_t index) const {
  static_assert(std::is_array_v<ObjectOrArrayType>,
                "UniquePtr<T>::operator[] is only allowed when T is an array");
  return get()[index];
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
bool UniquePtr<ObjectOrArrayType, AllocatorProviderT>::operator==(
    const UniquePtr<ObjectOrArrayType, AllocatorProviderT> &other) const {
  return mObject == other.get();
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
bool UniquePtr<ObjectOrArrayType, AllocatorProviderT>::operator!=(
    const UniquePtr<ObjectOrArrayType, AllocatorProviderT> &other) const {
  return !(*this == other);
}

template <typename ObjectOrArrayType, typename AllocatorProviderT>
UniquePtr<ObjectOrArrayType, AllocatorProviderT> &
UniquePtr<ObjectOrArrayType, AllocatorProviderT>::operator=(
    UniquePtr<ObjectOrArrayType, AllocatorProviderT> &&other) {
  reset();
  mObject = other.mObject;
  other.mObject = nullptr;
  return *this;
}

template <typename ObjectType, typename AllocatorProviderT, typename... Args>
inline UniquePtr<ObjectType, AllocatorProviderT> MakeUnique(Args &&...args) {
  UniquePtr<ObjectType, AllocatorProviderT> ptr;
  ptr.emplace(std::forward<Args>(args)...);
  return ptr;
}

template <typename ObjectArrayType, typename AllocatorProviderT>
UniquePtr<ObjectArrayType, AllocatorProviderT> MakeUniqueArray(size_t count) {
  UniquePtr<ObjectArrayType, AllocatorProviderT> ptr;
  ptr.makeArray(count);
  return ptr;
}

template <typename ObjectType, typename AllocatorProviderT>
inline UniquePtr<ObjectType, AllocatorProviderT> MakeUniqueZeroFill() {
  static_assert(std::is_trivial<ObjectType>::value,
                "MakeUniqueZeroFill is only supported for trivial types");
  UniquePtr<ObjectType, AllocatorProviderT> ptr;
  ptr.makeZeroFill();
  return ptr;
}

}  // namespace chre

#endif  // CHRE_UTIL_UNIQUE_PTR_IMPL_H_

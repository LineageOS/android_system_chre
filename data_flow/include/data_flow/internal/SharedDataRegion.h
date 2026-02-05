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

/// This file is a stripped-down version of the header SharedDataRegion.h
/// generated from the ContextHub HAL AIDL definitions for the NDK backend. It
/// contains the ABI for shared memory data structures relating to the
/// ContextHub Data Flow capabilities. It is necessary as AIDL code generation
/// is not currently available in CHRE.
///
/// If the ABI is updated in the AIDL, the contents of this file should be
/// replaced as follows:
/// 1. Build the NDK library from the latest AIDL to generate the base
///    SharedDataRegion.h
/// 2. Remove any members variables and methods of the SharedDataRegion class
/// 3. Remove any features that depend on the NDK or C++ standard library
///    features not supported by CHRE, specifically:
///    * All unsupported C++ headers
///    * All toString() methods
///    * All _aidl_stability fields
///    * All content in the ndk top-level namespace
/// 4. The remaining code should be copied below this line.

#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#ifndef __BIONIC__
#define __assert2(a, b, c, d) ((void)0)
#endif

namespace aidl {
namespace android {
namespace hardware {
namespace contexthub {
class SharedDataRegion {
 public:
  typedef std::false_type fixed_size;
  static const char *descriptor;

  class Version {
   public:
    typedef std::true_type fixed_size;
    static const char *descriptor;

    int8_t major __attribute__((aligned(1))) = 0;
    int8_t minor __attribute__((aligned(1))) = 0;
    char16_t patch __attribute__((aligned(2))) = '\0';

    inline bool operator==(const Version &_rhs) const {
      return std::tie(major, minor, patch) ==
             std::tie(_rhs.major, _rhs.minor, _rhs.patch);
    }
    inline bool operator<(const Version &_rhs) const {
      return std::tie(major, minor, patch) <
             std::tie(_rhs.major, _rhs.minor, _rhs.patch);
    }
    inline bool operator!=(const Version &_rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const Version &_rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const Version &_rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const Version &_rhs) const {
      return !(_rhs < *this);
    }
  };
  static_assert(offsetof(Version, major) == 0);
  static_assert(sizeof(int8_t) == 1);
  static_assert(offsetof(Version, minor) == 1);
  static_assert(sizeof(int8_t) == 1);
  static_assert(offsetof(Version, patch) == 2);
  static_assert(sizeof(char16_t) == 2);
  static_assert(alignof(Version) == 2);
  static_assert(sizeof(Version) == 4);
  class EndpointIdFixedSize {
   public:
    typedef std::true_type fixed_size;
    static const char *descriptor;

    int64_t hubId __attribute__((aligned(8))) = 0L;
    int64_t endpointId __attribute__((aligned(8))) = 0L;

    inline bool operator==(const EndpointIdFixedSize &_rhs) const {
      return std::tie(hubId, endpointId) ==
             std::tie(_rhs.hubId, _rhs.endpointId);
    }
    inline bool operator<(const EndpointIdFixedSize &_rhs) const {
      return std::tie(hubId, endpointId) <
             std::tie(_rhs.hubId, _rhs.endpointId);
    }
    inline bool operator!=(const EndpointIdFixedSize &_rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const EndpointIdFixedSize &_rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const EndpointIdFixedSize &_rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const EndpointIdFixedSize &_rhs) const {
      return !(_rhs < *this);
    }
  };
  static_assert(offsetof(EndpointIdFixedSize, hubId) == 0);
  static_assert(sizeof(int64_t) == 8);
  static_assert(offsetof(EndpointIdFixedSize, endpointId) == 8);
  static_assert(sizeof(int64_t) == 8);
  static_assert(alignof(EndpointIdFixedSize) == 8);
  static_assert(sizeof(EndpointIdFixedSize) == 16);
  class DataFlowElementConfig {
   public:
    typedef std::true_type fixed_size;
    static const char *descriptor;

    class FixedSize {
     public:
      typedef std::true_type fixed_size;
      static const char *descriptor;

      int32_t elementSizeBytes __attribute__((aligned(4))) = 0;
      char16_t elementAlignmentBytes __attribute__((aligned(2))) = '\0';
      std::array<uint8_t, 2> reserved __attribute__((aligned(1))) = {{}};

      inline bool operator==(const FixedSize &_rhs) const {
        return std::tie(elementSizeBytes, elementAlignmentBytes, reserved) ==
               std::tie(_rhs.elementSizeBytes, _rhs.elementAlignmentBytes,
                        _rhs.reserved);
      }
      inline bool operator<(const FixedSize &_rhs) const {
        return std::tie(elementSizeBytes, elementAlignmentBytes, reserved) <
               std::tie(_rhs.elementSizeBytes, _rhs.elementAlignmentBytes,
                        _rhs.reserved);
      }
      inline bool operator!=(const FixedSize &_rhs) const {
        return !(*this == _rhs);
      }
      inline bool operator>(const FixedSize &_rhs) const {
        return _rhs < *this;
      }
      inline bool operator>=(const FixedSize &_rhs) const {
        return !(*this < _rhs);
      }
      inline bool operator<=(const FixedSize &_rhs) const {
        return !(_rhs < *this);
      }
    };
    static_assert(offsetof(FixedSize, elementSizeBytes) == 0);
    static_assert(sizeof(int32_t) == 4);
    static_assert(offsetof(FixedSize, elementAlignmentBytes) == 4);
    static_assert(sizeof(char16_t) == 2);
    static_assert(offsetof(FixedSize, reserved) == 6);
    static_assert(sizeof(std::array<uint8_t, 2>) == 2);
    static_assert(alignof(FixedSize) == 4);
    static_assert(sizeof(FixedSize) == 8);
    class VariableSize {
     public:
      typedef std::true_type fixed_size;
      static const char *descriptor;

      char16_t elementAlignmentBytes __attribute__((aligned(2))) = '\0';
      std::array<uint8_t, 6> reserved __attribute__((aligned(1))) = {{}};

      inline bool operator==(const VariableSize &_rhs) const {
        return std::tie(elementAlignmentBytes, reserved) ==
               std::tie(_rhs.elementAlignmentBytes, _rhs.reserved);
      }
      inline bool operator<(const VariableSize &_rhs) const {
        return std::tie(elementAlignmentBytes, reserved) <
               std::tie(_rhs.elementAlignmentBytes, _rhs.reserved);
      }
      inline bool operator!=(const VariableSize &_rhs) const {
        return !(*this == _rhs);
      }
      inline bool operator>(const VariableSize &_rhs) const {
        return _rhs < *this;
      }
      inline bool operator>=(const VariableSize &_rhs) const {
        return !(*this < _rhs);
      }
      inline bool operator<=(const VariableSize &_rhs) const {
        return !(_rhs < *this);
      }
    };
    static_assert(offsetof(VariableSize, elementAlignmentBytes) == 0);
    static_assert(sizeof(char16_t) == 2);
    static_assert(offsetof(VariableSize, reserved) == 2);
    static_assert(sizeof(std::array<uint8_t, 6>) == 6);
    static_assert(alignof(VariableSize) == 2);
    static_assert(sizeof(VariableSize) == 8);
    enum class Tag : int8_t {
      fixedSize = 0,
      variableSize = 1,
    };

    // Expose tag symbols for legacy code
    static const inline Tag fixedSize = Tag::fixedSize;
    static const inline Tag variableSize = Tag::variableSize;

    template <Tag _Tag>
    using _at = typename std::tuple_element<
        static_cast<size_t>(_Tag),
        std::tuple<::aidl::android::hardware::contexthub::SharedDataRegion::
                       DataFlowElementConfig::FixedSize,
                   ::aidl::android::hardware::contexthub::SharedDataRegion::
                       DataFlowElementConfig::VariableSize>>::type;
    template <Tag _Tag, typename _Type>
    static DataFlowElementConfig make(_Type &&_arg) {
      DataFlowElementConfig _inst;
      _inst.set<_Tag>(std::forward<_Type>(_arg));
      return _inst;
    }
    constexpr Tag getTag() const {
      return _tag;
    }
    template <Tag _Tag>
    const _at<_Tag> &get() const {
      if (_Tag != _tag) {
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__,
                  "bad access: a wrong tag");
      }
      return *(_at<_Tag> *)(&_value);
    }
    template <Tag _Tag>
    _at<_Tag> &get() {
      if (_Tag != _tag) {
        __assert2(__FILE__, __LINE__, __PRETTY_FUNCTION__,
                  "bad access: a wrong tag");
      }
      return *(_at<_Tag> *)(&_value);
    }
    template <Tag _Tag, typename _Type>
    void set(_Type &&_arg) {
      _tag = _Tag;
      get<_Tag>() = std::forward<_Type>(_arg);
    }

    static int _cmp(const DataFlowElementConfig &_lhs,
                    const DataFlowElementConfig &_rhs) {
      return _cmp_value(_lhs.getTag(), _rhs.getTag()) ||
             _cmp_value_at<variableSize>(_lhs, _rhs);
    }
    template <Tag _Tag>
    static int _cmp_value_at(const DataFlowElementConfig &_lhs,
                             const DataFlowElementConfig &_rhs) {
      if constexpr (_Tag == fixedSize) {
        return _cmp_value(_lhs.get<_Tag>(), _rhs.get<_Tag>());
      } else {
        return (_lhs.getTag() == _Tag)
                   ? _cmp_value(_lhs.get<_Tag>(), _rhs.get<_Tag>())
                   : _cmp_value_at<static_cast<Tag>(static_cast<size_t>(_Tag) -
                                                    1)>(_lhs, _rhs);
      }
    }
    template <typename _Type>
    static int _cmp_value(const _Type &_lhs, const _Type &_rhs) {
      return (_lhs == _rhs) ? 0 : (_lhs < _rhs) ? -1 : 1;
    }
    inline bool operator!=(const DataFlowElementConfig &_rhs) const {
      return _cmp(*this, _rhs) != 0;
    }
    inline bool operator<(const DataFlowElementConfig &_rhs) const {
      return _cmp(*this, _rhs) < 0;
    }
    inline bool operator<=(const DataFlowElementConfig &_rhs) const {
      return _cmp(*this, _rhs) <= 0;
    }
    inline bool operator==(const DataFlowElementConfig &_rhs) const {
      return _cmp(*this, _rhs) == 0;
    }
    inline bool operator>(const DataFlowElementConfig &_rhs) const {
      return _cmp(*this, _rhs) > 0;
    }
    inline bool operator>=(const DataFlowElementConfig &_rhs) const {
      return _cmp(*this, _rhs) >= 0;
    }

   private:
    Tag _tag = fixedSize;
    uint8_t _zero_pad[3] __attribute__((unused)) = {};
    union _value_t {
      _value_t() {}
      ~_value_t() {}
      ::aidl::android::hardware::contexthub::SharedDataRegion::
          DataFlowElementConfig::FixedSize fixedSize
          __attribute__((aligned(4))) = ::aidl::android::hardware::contexthub::
              SharedDataRegion::DataFlowElementConfig::FixedSize();
      ::aidl::android::hardware::contexthub::SharedDataRegion::
          DataFlowElementConfig::VariableSize variableSize
          __attribute__((aligned(2)));
    } _value;
  };
  static_assert(sizeof(::aidl::android::hardware::contexthub::SharedDataRegion::
                           DataFlowElementConfig::FixedSize) == 8);
  static_assert(sizeof(::aidl::android::hardware::contexthub::SharedDataRegion::
                           DataFlowElementConfig::VariableSize) == 8);
  static_assert(alignof(DataFlowElementConfig) == 4);
  static_assert(sizeof(DataFlowElementConfig) == 12);
  class DataFlowMetadata {
   public:
    typedef std::true_type fixed_size;
    static const char *descriptor;

    ::aidl::android::hardware::contexthub::SharedDataRegion::Version version
        __attribute__((aligned(2)));
    int32_t sourceMetadataOffsetBytes __attribute__((aligned(4))) = 0;
    ::aidl::android::hardware::contexthub::SharedDataRegion::EndpointIdFixedSize
        sourceId __attribute__((aligned(8)));
    int32_t blockListEpoch __attribute__((aligned(4))) = 0;
    int32_t blockCapacityBytes __attribute__((aligned(4))) = 0;
    ::aidl::android::hardware::contexthub::SharedDataRegion::
        DataFlowElementConfig elementConfig __attribute__((aligned(4)));
    int8_t localNotify __attribute__((aligned(1))) = 0;
    std::array<uint8_t, 11> reserved __attribute__((aligned(1))) = {{}};

    inline bool operator==(const DataFlowMetadata &_rhs) const {
      return std::tie(version, sourceMetadataOffsetBytes, sourceId,
                      blockListEpoch, blockCapacityBytes, elementConfig,
                      localNotify, reserved) ==
             std::tie(_rhs.version, _rhs.sourceMetadataOffsetBytes,
                      _rhs.sourceId, _rhs.blockListEpoch,
                      _rhs.blockCapacityBytes, _rhs.elementConfig,
                      _rhs.localNotify, _rhs.reserved);
    }
    inline bool operator<(const DataFlowMetadata &_rhs) const {
      return std::tie(version, sourceMetadataOffsetBytes, sourceId,
                      blockListEpoch, blockCapacityBytes, elementConfig,
                      localNotify, reserved) <
             std::tie(_rhs.version, _rhs.sourceMetadataOffsetBytes,
                      _rhs.sourceId, _rhs.blockListEpoch,
                      _rhs.blockCapacityBytes, _rhs.elementConfig,
                      _rhs.localNotify, _rhs.reserved);
    }
    inline bool operator!=(const DataFlowMetadata &_rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const DataFlowMetadata &_rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const DataFlowMetadata &_rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const DataFlowMetadata &_rhs) const {
      return !(_rhs < *this);
    }
  };
  static_assert(offsetof(DataFlowMetadata, version) == 0);
  static_assert(
      sizeof(
          ::aidl::android::hardware::contexthub::SharedDataRegion::Version) ==
      4);
  static_assert(offsetof(DataFlowMetadata, sourceMetadataOffsetBytes) == 4);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowMetadata, sourceId) == 8);
  static_assert(sizeof(::aidl::android::hardware::contexthub::SharedDataRegion::
                           EndpointIdFixedSize) == 16);
  static_assert(offsetof(DataFlowMetadata, blockListEpoch) == 24);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowMetadata, blockCapacityBytes) == 28);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowMetadata, elementConfig) == 32);
  static_assert(sizeof(::aidl::android::hardware::contexthub::SharedDataRegion::
                           DataFlowElementConfig) == 12);
  static_assert(offsetof(DataFlowMetadata, localNotify) == 44);
  static_assert(sizeof(int8_t) == 1);
  static_assert(offsetof(DataFlowMetadata, reserved) == 45);
  static_assert(sizeof(std::array<uint8_t, 11>) == 11);
  static_assert(alignof(DataFlowMetadata) == 8);
  static_assert(sizeof(DataFlowMetadata) == 56);
  class DataFlowSourceMetadata {
   public:
    typedef std::true_type fixed_size;
    static const char *descriptor;

    int32_t writeIndex __attribute__((aligned(4))) = 0;
    int32_t indexCorrection __attribute__((aligned(4))) = 0;
    int32_t tailBlockOffsetBytes __attribute__((aligned(4))) = 0;
    std::array<uint8_t, 12> reserved __attribute__((aligned(1))) = {{}};

    inline bool operator==(const DataFlowSourceMetadata &_rhs) const {
      return std::tie(writeIndex, indexCorrection, tailBlockOffsetBytes,
                      reserved) ==
             std::tie(_rhs.writeIndex, _rhs.indexCorrection,
                      _rhs.tailBlockOffsetBytes, _rhs.reserved);
    }
    inline bool operator<(const DataFlowSourceMetadata &_rhs) const {
      return std::tie(writeIndex, indexCorrection, tailBlockOffsetBytes,
                      reserved) <
             std::tie(_rhs.writeIndex, _rhs.indexCorrection,
                      _rhs.tailBlockOffsetBytes, _rhs.reserved);
    }
    inline bool operator!=(const DataFlowSourceMetadata &_rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const DataFlowSourceMetadata &_rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const DataFlowSourceMetadata &_rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const DataFlowSourceMetadata &_rhs) const {
      return !(_rhs < *this);
    }
  };
  static_assert(offsetof(DataFlowSourceMetadata, writeIndex) == 0);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowSourceMetadata, indexCorrection) == 4);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowSourceMetadata, tailBlockOffsetBytes) == 8);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowSourceMetadata, reserved) == 12);
  static_assert(sizeof(std::array<uint8_t, 12>) == 12);
  static_assert(alignof(DataFlowSourceMetadata) == 4);
  static_assert(sizeof(DataFlowSourceMetadata) == 24);
  class DataFlowSinkMetadata {
   public:
    typedef std::true_type fixed_size;
    static const char *descriptor;

    enum class SourceFlags : int32_t {
      NONE = 0,
      PENDING_INIT = 1,
      BLOCKING = 2,
      OVERWRITE = 4,
      FINISHED = 8,
      DISCONNECTED = 16,
    };

    enum class SinkFlags : int32_t {
      CLEARED = 0,
      FINISHED = 1,
    };

    ::aidl::android::hardware::contexthub::SharedDataRegion::Version version
        __attribute__((aligned(2)));
    int32_t readIndex __attribute__((aligned(4))) = 0;
    int32_t indexCorrection __attribute__((aligned(4))) = 0;
    int32_t sourceFlags __attribute__((aligned(4))) = 0;
    ::aidl::android::hardware::contexthub::SharedDataRegion::EndpointIdFixedSize
        id __attribute__((aligned(8)));
    int32_t sinkFlags __attribute__((aligned(4))) = 0;
    int32_t initialHeadBlockOffsetBytes __attribute__((aligned(4))) = 0;
    int32_t initialBlockListEpoch __attribute__((aligned(4))) = 0;
    bool isOverwritable __attribute__((aligned(1))) = false;
    std::array<uint8_t, 11> reserved __attribute__((aligned(1))) = {{}};

    inline bool operator==(const DataFlowSinkMetadata &_rhs) const {
      return std::tie(version, readIndex, indexCorrection, sourceFlags, id,
                      sinkFlags, initialHeadBlockOffsetBytes,
                      initialBlockListEpoch, isOverwritable, reserved) ==
             std::tie(_rhs.version, _rhs.readIndex, _rhs.indexCorrection,
                      _rhs.sourceFlags, _rhs.id, _rhs.sinkFlags,
                      _rhs.initialHeadBlockOffsetBytes,
                      _rhs.initialBlockListEpoch, _rhs.isOverwritable,
                      _rhs.reserved);
    }
    inline bool operator<(const DataFlowSinkMetadata &_rhs) const {
      return std::tie(version, readIndex, indexCorrection, sourceFlags, id,
                      sinkFlags, initialHeadBlockOffsetBytes,
                      initialBlockListEpoch, isOverwritable, reserved) <
             std::tie(_rhs.version, _rhs.readIndex, _rhs.indexCorrection,
                      _rhs.sourceFlags, _rhs.id, _rhs.sinkFlags,
                      _rhs.initialHeadBlockOffsetBytes,
                      _rhs.initialBlockListEpoch, _rhs.isOverwritable,
                      _rhs.reserved);
    }
    inline bool operator!=(const DataFlowSinkMetadata &_rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const DataFlowSinkMetadata &_rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const DataFlowSinkMetadata &_rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const DataFlowSinkMetadata &_rhs) const {
      return !(_rhs < *this);
    }
  };
  static_assert(offsetof(DataFlowSinkMetadata, version) == 0);
  static_assert(
      sizeof(
          ::aidl::android::hardware::contexthub::SharedDataRegion::Version) ==
      4);
  static_assert(offsetof(DataFlowSinkMetadata, readIndex) == 4);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowSinkMetadata, indexCorrection) == 8);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowSinkMetadata, sourceFlags) == 12);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowSinkMetadata, id) == 16);
  static_assert(sizeof(::aidl::android::hardware::contexthub::SharedDataRegion::
                           EndpointIdFixedSize) == 16);
  static_assert(offsetof(DataFlowSinkMetadata, sinkFlags) == 32);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowSinkMetadata, initialHeadBlockOffsetBytes) ==
                36);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowSinkMetadata, initialBlockListEpoch) == 40);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowSinkMetadata, isOverwritable) == 44);
  static_assert(sizeof(bool) == 1);
  static_assert(offsetof(DataFlowSinkMetadata, reserved) == 45);
  static_assert(sizeof(std::array<uint8_t, 11>) == 11);
  static_assert(alignof(DataFlowSinkMetadata) == 8);
  static_assert(sizeof(DataFlowSinkMetadata) == 56);
  class DataFlowBlockHeader {
   public:
    typedef std::true_type fixed_size;
    static const char *descriptor;

    ::aidl::android::hardware::contexthub::SharedDataRegion::
        DataFlowSourceMetadata sourceMetadata __attribute__((aligned(4)));
    int32_t nextBlockOffsetBytes __attribute__((aligned(4))) = 0;
    int32_t baseIndex __attribute__((aligned(4))) = 0;
    int32_t skipIndex __attribute__((aligned(4))) = 0;
    std::array<uint8_t, 12> reserved __attribute__((aligned(1))) = {{}};

    inline bool operator==(const DataFlowBlockHeader &_rhs) const {
      return std::tie(sourceMetadata, nextBlockOffsetBytes, baseIndex,
                      skipIndex, reserved) ==
             std::tie(_rhs.sourceMetadata, _rhs.nextBlockOffsetBytes,
                      _rhs.baseIndex, _rhs.skipIndex, _rhs.reserved);
    }
    inline bool operator<(const DataFlowBlockHeader &_rhs) const {
      return std::tie(sourceMetadata, nextBlockOffsetBytes, baseIndex,
                      skipIndex, reserved) <
             std::tie(_rhs.sourceMetadata, _rhs.nextBlockOffsetBytes,
                      _rhs.baseIndex, _rhs.skipIndex, _rhs.reserved);
    }
    inline bool operator!=(const DataFlowBlockHeader &_rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const DataFlowBlockHeader &_rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const DataFlowBlockHeader &_rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const DataFlowBlockHeader &_rhs) const {
      return !(_rhs < *this);
    }
  };
  static_assert(offsetof(DataFlowBlockHeader, sourceMetadata) == 0);
  static_assert(sizeof(::aidl::android::hardware::contexthub::SharedDataRegion::
                           DataFlowSourceMetadata) == 24);
  static_assert(offsetof(DataFlowBlockHeader, nextBlockOffsetBytes) == 24);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowBlockHeader, baseIndex) == 28);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowBlockHeader, skipIndex) == 32);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowBlockHeader, reserved) == 36);
  static_assert(sizeof(std::array<uint8_t, 12>) == 12);
  static_assert(alignof(DataFlowBlockHeader) == 4);
  static_assert(sizeof(DataFlowBlockHeader) == 48);
  class DataFlowVariableSizeBlockHeader {
   public:
    typedef std::true_type fixed_size;
    static const char *descriptor;

    ::aidl::android::hardware::contexthub::SharedDataRegion::DataFlowBlockHeader
        blockHeader __attribute__((aligned(4)));
    int32_t firstElementIndex __attribute__((aligned(4))) = 0;
    std::array<uint8_t, 12> reserved __attribute__((aligned(1))) = {{}};

    inline bool operator==(const DataFlowVariableSizeBlockHeader &_rhs) const {
      return std::tie(blockHeader, firstElementIndex, reserved) ==
             std::tie(_rhs.blockHeader, _rhs.firstElementIndex, _rhs.reserved);
    }
    inline bool operator<(const DataFlowVariableSizeBlockHeader &_rhs) const {
      return std::tie(blockHeader, firstElementIndex, reserved) <
             std::tie(_rhs.blockHeader, _rhs.firstElementIndex, _rhs.reserved);
    }
    inline bool operator!=(const DataFlowVariableSizeBlockHeader &_rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const DataFlowVariableSizeBlockHeader &_rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(const DataFlowVariableSizeBlockHeader &_rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(const DataFlowVariableSizeBlockHeader &_rhs) const {
      return !(_rhs < *this);
    }
  };
  static_assert(offsetof(DataFlowVariableSizeBlockHeader, blockHeader) == 0);
  static_assert(sizeof(::aidl::android::hardware::contexthub::SharedDataRegion::
                           DataFlowBlockHeader) == 48);
  static_assert(offsetof(DataFlowVariableSizeBlockHeader, firstElementIndex) ==
                48);
  static_assert(sizeof(int32_t) == 4);
  static_assert(offsetof(DataFlowVariableSizeBlockHeader, reserved) == 52);
  static_assert(sizeof(std::array<uint8_t, 12>) == 12);
  static_assert(alignof(DataFlowVariableSizeBlockHeader) == 4);
  static_assert(sizeof(DataFlowVariableSizeBlockHeader) == 64);
  class DataFlowVariableSizeElementHeader {
   public:
    typedef std::true_type fixed_size;
    static const char *descriptor;

    int32_t sizeBytes __attribute__((aligned(4))) = 0;

    inline bool operator==(
        const DataFlowVariableSizeElementHeader &_rhs) const {
      return std::tie(sizeBytes) == std::tie(_rhs.sizeBytes);
    }
    inline bool operator<(const DataFlowVariableSizeElementHeader &_rhs) const {
      return std::tie(sizeBytes) < std::tie(_rhs.sizeBytes);
    }
    inline bool operator!=(
        const DataFlowVariableSizeElementHeader &_rhs) const {
      return !(*this == _rhs);
    }
    inline bool operator>(const DataFlowVariableSizeElementHeader &_rhs) const {
      return _rhs < *this;
    }
    inline bool operator>=(
        const DataFlowVariableSizeElementHeader &_rhs) const {
      return !(*this < _rhs);
    }
    inline bool operator<=(
        const DataFlowVariableSizeElementHeader &_rhs) const {
      return !(_rhs < *this);
    }
  };
  static_assert(offsetof(DataFlowVariableSizeElementHeader, sizeBytes) == 0);
  static_assert(sizeof(int32_t) == 4);
  static_assert(alignof(DataFlowVariableSizeElementHeader) == 4);
  static_assert(sizeof(DataFlowVariableSizeElementHeader) == 4);

  enum : int32_t { OFFSET_INVALID = -1 };
};
}  // namespace contexthub
}  // namespace hardware
}  // namespace android
}  // namespace aidl

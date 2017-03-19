// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_HOSTMESSAGES_CHRE_FBS_H_
#define FLATBUFFERS_GENERATED_HOSTMESSAGES_CHRE_FBS_H_

#include "flatbuffers/flatbuffers.h"

namespace chre {
namespace fbs {

struct NanoappMessage;

struct HubInfoRequest;

struct HubInfoResponse;

struct MessageContainer;

/// A union that joins together all possible messages. Note that in FlatBuffers,
/// unions have an implicit type
enum class ChreMessage : uint8_t {
  NONE = 0,
  NanoappMessage = 1,
  HubInfoRequest = 2,
  HubInfoResponse = 3,
  MIN = NONE,
  MAX = HubInfoResponse
};

inline const char **EnumNamesChreMessage() {
  static const char *names[] = {
    "NONE",
    "NanoappMessage",
    "HubInfoRequest",
    "HubInfoResponse",
    nullptr
  };
  return names;
}

inline const char *EnumNameChreMessage(ChreMessage e) {
  const size_t index = static_cast<int>(e);
  return EnumNamesChreMessage()[index];
}

template<typename T> struct ChreMessageTraits {
  static const ChreMessage enum_value = ChreMessage::NONE;
};

template<> struct ChreMessageTraits<NanoappMessage> {
  static const ChreMessage enum_value = ChreMessage::NanoappMessage;
};

template<> struct ChreMessageTraits<HubInfoRequest> {
  static const ChreMessage enum_value = ChreMessage::HubInfoRequest;
};

template<> struct ChreMessageTraits<HubInfoResponse> {
  static const ChreMessage enum_value = ChreMessage::HubInfoResponse;
};

bool VerifyChreMessage(flatbuffers::Verifier &verifier, const void *obj, ChreMessage type);
bool VerifyChreMessageVector(flatbuffers::Verifier &verifier, const flatbuffers::Vector<flatbuffers::Offset<void>> *values, const flatbuffers::Vector<uint8_t> *types);

/// Represents a message sent to/from a nanoapp from/to a client on the host
struct NanoappMessage FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_APP_ID = 4,
    VT_MESSAGE_TYPE = 6,
    VT_HOST_ENDPOINT = 8,
    VT_MESSAGE = 10
  };
  uint64_t app_id() const {
    return GetField<uint64_t>(VT_APP_ID, 0);
  }
  uint32_t message_type() const {
    return GetField<uint32_t>(VT_MESSAGE_TYPE, 0);
  }
  /// Identifies the host-side endpoint on the host that sent or should receive
  /// this message. The default value is a special value defined in the HAL and
  /// elsewhere that indicates that the endpoint is unspecified.
  uint16_t host_endpoint() const {
    return GetField<uint16_t>(VT_HOST_ENDPOINT, 65534);
  }
  /// Vector containing arbitrary application-specific message data
  const flatbuffers::Vector<uint8_t> *message() const {
    return GetPointer<const flatbuffers::Vector<uint8_t> *>(VT_MESSAGE);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<uint64_t>(verifier, VT_APP_ID) &&
           VerifyField<uint32_t>(verifier, VT_MESSAGE_TYPE) &&
           VerifyField<uint16_t>(verifier, VT_HOST_ENDPOINT) &&
           VerifyFieldRequired<flatbuffers::uoffset_t>(verifier, VT_MESSAGE) &&
           verifier.Verify(message()) &&
           verifier.EndTable();
  }
};

struct NanoappMessageBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_app_id(uint64_t app_id) {
    fbb_.AddElement<uint64_t>(NanoappMessage::VT_APP_ID, app_id, 0);
  }
  void add_message_type(uint32_t message_type) {
    fbb_.AddElement<uint32_t>(NanoappMessage::VT_MESSAGE_TYPE, message_type, 0);
  }
  void add_host_endpoint(uint16_t host_endpoint) {
    fbb_.AddElement<uint16_t>(NanoappMessage::VT_HOST_ENDPOINT, host_endpoint, 65534);
  }
  void add_message(flatbuffers::Offset<flatbuffers::Vector<uint8_t>> message) {
    fbb_.AddOffset(NanoappMessage::VT_MESSAGE, message);
  }
  NanoappMessageBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  NanoappMessageBuilder &operator=(const NanoappMessageBuilder &);
  flatbuffers::Offset<NanoappMessage> Finish() {
    const auto end = fbb_.EndTable(start_, 4);
    auto o = flatbuffers::Offset<NanoappMessage>(end);
    fbb_.Required(o, NanoappMessage::VT_MESSAGE);
    return o;
  }
};

inline flatbuffers::Offset<NanoappMessage> CreateNanoappMessage(
    flatbuffers::FlatBufferBuilder &_fbb,
    uint64_t app_id = 0,
    uint32_t message_type = 0,
    uint16_t host_endpoint = 65534,
    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> message = 0) {
  NanoappMessageBuilder builder_(_fbb);
  builder_.add_app_id(app_id);
  builder_.add_message(message);
  builder_.add_message_type(message_type);
  builder_.add_host_endpoint(host_endpoint);
  return builder_.Finish();
}

inline flatbuffers::Offset<NanoappMessage> CreateNanoappMessageDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    uint64_t app_id = 0,
    uint32_t message_type = 0,
    uint16_t host_endpoint = 65534,
    const std::vector<uint8_t> *message = nullptr) {
  return chre::fbs::CreateNanoappMessage(
      _fbb,
      app_id,
      message_type,
      host_endpoint,
      message ? _fbb.CreateVector<uint8_t>(*message) : 0);
}

struct HubInfoRequest FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           verifier.EndTable();
  }
};

struct HubInfoRequestBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  HubInfoRequestBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  HubInfoRequestBuilder &operator=(const HubInfoRequestBuilder &);
  flatbuffers::Offset<HubInfoRequest> Finish() {
    const auto end = fbb_.EndTable(start_, 0);
    auto o = flatbuffers::Offset<HubInfoRequest>(end);
    return o;
  }
};

inline flatbuffers::Offset<HubInfoRequest> CreateHubInfoRequest(
    flatbuffers::FlatBufferBuilder &_fbb) {
  HubInfoRequestBuilder builder_(_fbb);
  return builder_.Finish();
}

struct HubInfoResponse FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_NAME = 4,
    VT_VENDOR = 6,
    VT_TOOLCHAIN = 8,
    VT_PLATFORM_VERSION = 10,
    VT_TOOLCHAIN_VERSION = 12,
    VT_PEAK_MIPS = 14,
    VT_STOPPED_POWER = 16,
    VT_SLEEP_POWER = 18,
    VT_PEAK_POWER = 20,
    VT_MAX_MSG_LEN = 22,
    VT_PLATFORM_ID = 24,
    VT_CHRE_PLATFORM_VERSION = 26
  };
  /// The name of the hub. Nominally a UTF-8 string, but note that we're not
  /// using the built-in "string" data type from FlatBuffers here, because the
  /// generated C++ uses std::string which is not well-supported in CHRE. This
  /// applies for vendor and toolchain as well.
  const flatbuffers::Vector<int8_t> *name() const {
    return GetPointer<const flatbuffers::Vector<int8_t> *>(VT_NAME);
  }
  const flatbuffers::Vector<int8_t> *vendor() const {
    return GetPointer<const flatbuffers::Vector<int8_t> *>(VT_VENDOR);
  }
  const flatbuffers::Vector<int8_t> *toolchain() const {
    return GetPointer<const flatbuffers::Vector<int8_t> *>(VT_TOOLCHAIN);
  }
  /// Legacy platform version reported in the HAL; semantics not strictly
  /// defined
  uint32_t platform_version() const {
    return GetField<uint32_t>(VT_PLATFORM_VERSION, 0);
  }
  /// Toolchain version reported in the HAL; semantics not strictly defined
  uint32_t toolchain_version() const {
    return GetField<uint32_t>(VT_TOOLCHAIN_VERSION, 0);
  }
  float peak_mips() const {
    return GetField<float>(VT_PEAK_MIPS, 0.0f);
  }
  float stopped_power() const {
    return GetField<float>(VT_STOPPED_POWER, 0.0f);
  }
  float sleep_power() const {
    return GetField<float>(VT_SLEEP_POWER, 0.0f);
  }
  float peak_power() const {
    return GetField<float>(VT_PEAK_POWER, 0.0f);
  }
  /// Maximum size message that can be sent to a nanoapp
  uint32_t max_msg_len() const {
    return GetField<uint32_t>(VT_MAX_MSG_LEN, 0);
  }
  /// @see chreGetPlatformId()
  uint64_t platform_id() const {
    return GetField<uint64_t>(VT_PLATFORM_ID, 0);
  }
  /// @see chreGetVersion()
  uint32_t chre_platform_version() const {
    return GetField<uint32_t>(VT_CHRE_PLATFORM_VERSION, 0);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<flatbuffers::uoffset_t>(verifier, VT_NAME) &&
           verifier.Verify(name()) &&
           VerifyField<flatbuffers::uoffset_t>(verifier, VT_VENDOR) &&
           verifier.Verify(vendor()) &&
           VerifyField<flatbuffers::uoffset_t>(verifier, VT_TOOLCHAIN) &&
           verifier.Verify(toolchain()) &&
           VerifyField<uint32_t>(verifier, VT_PLATFORM_VERSION) &&
           VerifyField<uint32_t>(verifier, VT_TOOLCHAIN_VERSION) &&
           VerifyField<float>(verifier, VT_PEAK_MIPS) &&
           VerifyField<float>(verifier, VT_STOPPED_POWER) &&
           VerifyField<float>(verifier, VT_SLEEP_POWER) &&
           VerifyField<float>(verifier, VT_PEAK_POWER) &&
           VerifyField<uint32_t>(verifier, VT_MAX_MSG_LEN) &&
           VerifyField<uint64_t>(verifier, VT_PLATFORM_ID) &&
           VerifyField<uint32_t>(verifier, VT_CHRE_PLATFORM_VERSION) &&
           verifier.EndTable();
  }
};

struct HubInfoResponseBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_name(flatbuffers::Offset<flatbuffers::Vector<int8_t>> name) {
    fbb_.AddOffset(HubInfoResponse::VT_NAME, name);
  }
  void add_vendor(flatbuffers::Offset<flatbuffers::Vector<int8_t>> vendor) {
    fbb_.AddOffset(HubInfoResponse::VT_VENDOR, vendor);
  }
  void add_toolchain(flatbuffers::Offset<flatbuffers::Vector<int8_t>> toolchain) {
    fbb_.AddOffset(HubInfoResponse::VT_TOOLCHAIN, toolchain);
  }
  void add_platform_version(uint32_t platform_version) {
    fbb_.AddElement<uint32_t>(HubInfoResponse::VT_PLATFORM_VERSION, platform_version, 0);
  }
  void add_toolchain_version(uint32_t toolchain_version) {
    fbb_.AddElement<uint32_t>(HubInfoResponse::VT_TOOLCHAIN_VERSION, toolchain_version, 0);
  }
  void add_peak_mips(float peak_mips) {
    fbb_.AddElement<float>(HubInfoResponse::VT_PEAK_MIPS, peak_mips, 0.0f);
  }
  void add_stopped_power(float stopped_power) {
    fbb_.AddElement<float>(HubInfoResponse::VT_STOPPED_POWER, stopped_power, 0.0f);
  }
  void add_sleep_power(float sleep_power) {
    fbb_.AddElement<float>(HubInfoResponse::VT_SLEEP_POWER, sleep_power, 0.0f);
  }
  void add_peak_power(float peak_power) {
    fbb_.AddElement<float>(HubInfoResponse::VT_PEAK_POWER, peak_power, 0.0f);
  }
  void add_max_msg_len(uint32_t max_msg_len) {
    fbb_.AddElement<uint32_t>(HubInfoResponse::VT_MAX_MSG_LEN, max_msg_len, 0);
  }
  void add_platform_id(uint64_t platform_id) {
    fbb_.AddElement<uint64_t>(HubInfoResponse::VT_PLATFORM_ID, platform_id, 0);
  }
  void add_chre_platform_version(uint32_t chre_platform_version) {
    fbb_.AddElement<uint32_t>(HubInfoResponse::VT_CHRE_PLATFORM_VERSION, chre_platform_version, 0);
  }
  HubInfoResponseBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  HubInfoResponseBuilder &operator=(const HubInfoResponseBuilder &);
  flatbuffers::Offset<HubInfoResponse> Finish() {
    const auto end = fbb_.EndTable(start_, 12);
    auto o = flatbuffers::Offset<HubInfoResponse>(end);
    return o;
  }
};

inline flatbuffers::Offset<HubInfoResponse> CreateHubInfoResponse(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::Vector<int8_t>> name = 0,
    flatbuffers::Offset<flatbuffers::Vector<int8_t>> vendor = 0,
    flatbuffers::Offset<flatbuffers::Vector<int8_t>> toolchain = 0,
    uint32_t platform_version = 0,
    uint32_t toolchain_version = 0,
    float peak_mips = 0.0f,
    float stopped_power = 0.0f,
    float sleep_power = 0.0f,
    float peak_power = 0.0f,
    uint32_t max_msg_len = 0,
    uint64_t platform_id = 0,
    uint32_t chre_platform_version = 0) {
  HubInfoResponseBuilder builder_(_fbb);
  builder_.add_platform_id(platform_id);
  builder_.add_chre_platform_version(chre_platform_version);
  builder_.add_max_msg_len(max_msg_len);
  builder_.add_peak_power(peak_power);
  builder_.add_sleep_power(sleep_power);
  builder_.add_stopped_power(stopped_power);
  builder_.add_peak_mips(peak_mips);
  builder_.add_toolchain_version(toolchain_version);
  builder_.add_platform_version(platform_version);
  builder_.add_toolchain(toolchain);
  builder_.add_vendor(vendor);
  builder_.add_name(name);
  return builder_.Finish();
}

inline flatbuffers::Offset<HubInfoResponse> CreateHubInfoResponseDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const std::vector<int8_t> *name = nullptr,
    const std::vector<int8_t> *vendor = nullptr,
    const std::vector<int8_t> *toolchain = nullptr,
    uint32_t platform_version = 0,
    uint32_t toolchain_version = 0,
    float peak_mips = 0.0f,
    float stopped_power = 0.0f,
    float sleep_power = 0.0f,
    float peak_power = 0.0f,
    uint32_t max_msg_len = 0,
    uint64_t platform_id = 0,
    uint32_t chre_platform_version = 0) {
  return chre::fbs::CreateHubInfoResponse(
      _fbb,
      name ? _fbb.CreateVector<int8_t>(*name) : 0,
      vendor ? _fbb.CreateVector<int8_t>(*vendor) : 0,
      toolchain ? _fbb.CreateVector<int8_t>(*toolchain) : 0,
      platform_version,
      toolchain_version,
      peak_mips,
      stopped_power,
      sleep_power,
      peak_power,
      max_msg_len,
      platform_id,
      chre_platform_version);
}

/// The top-level container that encapsulates all possible messages. Note that
/// per FlatBuffers requirements, we can't use a union as the top-level structure
/// (root type), so we must wrap it in a table.
struct MessageContainer FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum {
    VT_MESSAGE_TYPE = 4,
    VT_MESSAGE = 6
  };
  ChreMessage message_type() const {
    return static_cast<ChreMessage>(GetField<uint8_t>(VT_MESSAGE_TYPE, 0));
  }
  const void *message() const {
    return GetPointer<const void *>(VT_MESSAGE);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<uint8_t>(verifier, VT_MESSAGE_TYPE) &&
           VerifyFieldRequired<flatbuffers::uoffset_t>(verifier, VT_MESSAGE) &&
           VerifyChreMessage(verifier, message(), message_type()) &&
           verifier.EndTable();
  }
};

struct MessageContainerBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_message_type(ChreMessage message_type) {
    fbb_.AddElement<uint8_t>(MessageContainer::VT_MESSAGE_TYPE, static_cast<uint8_t>(message_type), 0);
  }
  void add_message(flatbuffers::Offset<void> message) {
    fbb_.AddOffset(MessageContainer::VT_MESSAGE, message);
  }
  MessageContainerBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  MessageContainerBuilder &operator=(const MessageContainerBuilder &);
  flatbuffers::Offset<MessageContainer> Finish() {
    const auto end = fbb_.EndTable(start_, 2);
    auto o = flatbuffers::Offset<MessageContainer>(end);
    fbb_.Required(o, MessageContainer::VT_MESSAGE);
    return o;
  }
};

inline flatbuffers::Offset<MessageContainer> CreateMessageContainer(
    flatbuffers::FlatBufferBuilder &_fbb,
    ChreMessage message_type = ChreMessage::NONE,
    flatbuffers::Offset<void> message = 0) {
  MessageContainerBuilder builder_(_fbb);
  builder_.add_message(message);
  builder_.add_message_type(message_type);
  return builder_.Finish();
}

inline bool VerifyChreMessage(flatbuffers::Verifier &verifier, const void *obj, ChreMessage type) {
  switch (type) {
    case ChreMessage::NONE: {
      return true;
    }
    case ChreMessage::NanoappMessage: {
      auto ptr = reinterpret_cast<const NanoappMessage *>(obj);
      return verifier.VerifyTable(ptr);
    }
    case ChreMessage::HubInfoRequest: {
      auto ptr = reinterpret_cast<const HubInfoRequest *>(obj);
      return verifier.VerifyTable(ptr);
    }
    case ChreMessage::HubInfoResponse: {
      auto ptr = reinterpret_cast<const HubInfoResponse *>(obj);
      return verifier.VerifyTable(ptr);
    }
    default: return false;
  }
}

inline bool VerifyChreMessageVector(flatbuffers::Verifier &verifier, const flatbuffers::Vector<flatbuffers::Offset<void>> *values, const flatbuffers::Vector<uint8_t> *types) {
  if (values->size() != types->size()) return false;
  for (flatbuffers::uoffset_t i = 0; i < values->size(); ++i) {
    if (!VerifyChreMessage(
        verifier,  values->Get(i), types->GetEnum<ChreMessage>(i))) {
      return false;
    }
  }
  return true;
}

inline const chre::fbs::MessageContainer *GetMessageContainer(const void *buf) {
  return flatbuffers::GetRoot<chre::fbs::MessageContainer>(buf);
}

inline bool VerifyMessageContainerBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<chre::fbs::MessageContainer>(nullptr);
}

inline void FinishMessageContainerBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<chre::fbs::MessageContainer> root) {
  fbb.Finish(root);
}

}  // namespace fbs
}  // namespace chre

#endif  // FLATBUFFERS_GENERATED_HOSTMESSAGES_CHRE_FBS_H_

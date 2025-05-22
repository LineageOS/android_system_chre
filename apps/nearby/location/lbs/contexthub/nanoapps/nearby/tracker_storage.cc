#include "location/lbs/contexthub/nanoapps/nearby/tracker_storage.h"

#include <inttypes.h>

#include <cstdint>
#include <cstring>
#include <utility>

#include "chre_api/chre.h"
#include "location/lbs/contexthub/nanoapps/nearby/hw_filter.h"
#include "third_party/contexthub/chre/util/include/chre/util/nanoapp/log.h"
#include "third_party/contexthub/chre/util/include/chre/util/time.h"
#include "third_party/contexthub/chre/util/include/chre/util/unique_ptr.h"

#define LOG_TAG "[NEARBY][TRACKER_STORAGE]"

namespace nearby {
namespace {
constexpr chreBleGenericFilter kDultTagGenericFilter = {
    .type = CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16_LE,
    .len = 2,
    .data = {0xb2, 0xfc},
    .dataMask = {0xff, 0xff},
};

inline bool IsDultTagAdvertisingData(const uint8_t *data, uint16_t length) {
  if (data == nullptr) {
    return false;
  }
  chreBleAdvertisingReport report = {};
  report.dataLength = length;
  report.data = data;
  return HwFilter::Match(kDultTagGenericFilter, report);
}
}  // namespace

void TrackerStorage::Push(const chreBleAdvertisingReport &report,
                          const TrackerBatchConfig &config) {
  for (auto &tracker_report : tracker_reports_) {
    if (IsEqualAddress(tracker_report, report)) {
      UpdateTrackerReport(tracker_report, config, report);
      return;
    }
  }
  AddTrackerReport(report, config);
}

void TrackerStorage::Refresh(const TrackerBatchConfig &config) {
  uint32_t current_time_ms = GetCurrentTimeMs();
  if (config.lost_timeout_ms == 0) {
    return;
  }
  for (auto &tracker_report : tracker_reports_) {
    if (tracker_report.historian.empty()) {
      LOGW("Empty tracker history found in tracker report");
      continue;
    }
    TrackerHistory &back = tracker_report.historian.back();
    if (back.state != TrackerState::kPresent) {
      continue;
    }
    if (current_time_ms >=
        back.last_radio_discovery_time_ms + config.lost_timeout_ms) {
      back.state = TrackerState::kAbsent;
      back.lost_time_ms = current_time_ms;
    }
  }
}

void TrackerStorage::UpdateTrackerReport(
    TrackerReport &tracker_report, const TrackerBatchConfig &config,
    const chreBleAdvertisingReport &report) {
  LOGD_SENSITIVE_INFO(
      "Received tracker report, tracker address: %02X:%02X:%02X:%02X:%02X:%02X",
      tracker_report.header.address[0], tracker_report.header.address[1],
      tracker_report.header.address[2], tracker_report.header.address[3],
      tracker_report.header.address[4], tracker_report.header.address[5]);
  uint32_t current_time_ms = GetCurrentTimeMs();
  if (tracker_report.historian.empty() ||
      tracker_report.historian.back().state != TrackerState::kPresent) {
    // Adds a new history.
    tracker_report.historian.emplace_back(TrackerHistory(current_time_ms));
  } else {
    // Updates the history every sampling interval.
    if (current_time_ms >= tracker_report.historian.back().last_found_time_ms +
                               config.sample_interval_ms) {
      tracker_report.historian.back().found_count++;
      tracker_report.historian.back().last_found_time_ms = current_time_ms;
    }
    // Updates the last radio discovery time in the history without sampling.
    tracker_report.historian.back().last_radio_discovery_time_ms =
        current_time_ms;
  }
  // Updates the advertising data if it is different from the previous one.
  AddOrUpdateAdvertisingData(tracker_report, report);
  if (tracker_report.historian.size() > config.max_history_count) {
    LOGW(
        "Discarding old tracker history. Tracker history count %zu max history "
        "count %" PRId32,
        tracker_report.historian.size(), config.max_history_count);
    // Discards the oldest history. It is important to keep the order because
    // the history at the end is always updated.
    // TODO(b/341757839): Optimize tracker storage memory by refreshing and
    // merging the tracker reports and histories.
    tracker_report.historian.erase(0);
  }
}

void TrackerStorage::AddTrackerReport(const chreBleAdvertisingReport &report,
                                      const TrackerBatchConfig &config) {
  // Doesn't add a new tracker report if the max count has already been
  // reached. Instead, it reports the storage full event to host when the
  // notification level has been reached so that it can flush the tracker batch
  // reports in advance.
  size_t tracker_count = tracker_reports_.size();
  if (tracker_count >= config.notify_threshold_tracker_count) {
    if (callback_ != nullptr) {
      callback_->OnTrackerStorageFullEvent();
    }
    if (tracker_count >= config.max_tracker_count) {
      LOGW("There are too many trackers. Tracker count %zu max count %" PRId32,
           tracker_reports_.size(), config.max_tracker_count);
      return;
    }
  }
  // Creates a new key report and copies header.
  TrackerReport new_report;
  // Adds the advertising data to the new tracker report.
  AddOrUpdateAdvertisingData(new_report, report);
  // For the new report, add a tracker history.
  new_report.historian.reserve(kDefaultTrackerHistorySize);
  new_report.historian.emplace_back(TrackerHistory(GetCurrentTimeMs()));
  if (!tracker_reports_.push_back(std::move(new_report))) {
    LOGE("Pushes a new tracker report failed!");
  }
  LOGD("Tracker count %zu notify count %" PRId32 " max count %" PRId32,
       tracker_reports_.size(), config.notify_threshold_tracker_count,
       config.max_tracker_count);
}

void TrackerStorage::AddOrUpdateAdvertisingData(
    TrackerReport &tracker_report, const chreBleAdvertisingReport &report) {
  uint16_t dataLength = report.dataLength;
  if (dataLength <= 0) {
    LOGW("Empty advertising data found in advertising report");
    return;
  }
  // If the advertising data is the same as the previous one or exempt from
  // updating advertising data, it will not do anything.
  if (tracker_report.data != nullptr &&
      ((tracker_report.header.dataLength == dataLength &&
        memcmp(tracker_report.data.get(), report.data,
               tracker_report.header.dataLength) == 0) ||
       IsExemptFromUpdateAdvertisingData(tracker_report, report))) {
    return;
  }
  if (tracker_report.data == nullptr ||
      tracker_report.header.dataLength != dataLength) {
    // Allocates advertise data and copy it as well.
    chre::UniquePtr<uint8_t[]> data =
        chre::MakeUniqueArray<uint8_t[]>(dataLength);
    if (data == nullptr) {
      LOGE("Memory allocation failed!");
      return;
    }
    tracker_report.data = std::move(data);
  }
  tracker_report.header = report;
  memcpy(tracker_report.data.get(), report.data,
         tracker_report.header.dataLength);
  tracker_report.header.data = tracker_report.data.get();
}

bool TrackerStorage::IsExemptFromUpdateAdvertisingData(
    const TrackerReport &tracker_report,
    const chreBleAdvertisingReport &report) {
  // For some tag devices, which alternate between legacy and DULT advertising
  // formats using the same mac address, we want to prioritize and retain the
  // DULT advertising data. If the existing tracker report contains DULT
  // advertising data and the new report doesn't, we keep the existing DULT
  // advertising data and ignore the new report by returning true so that
  // exempting from updating advertising data.
  return IsDultTagAdvertisingData(tracker_report.data.get(),
                                  tracker_report.header.dataLength) &&
         !IsDultTagAdvertisingData(report.data, report.dataLength);
}

bool TrackerStorage::IsEqualAddress(
    const TrackerReport &tracker_report,
    const chreBleAdvertisingReport &report) const {
  return (tracker_report.header.addressType == report.addressType &&
          memcmp(tracker_report.header.address, report.address,
                 CHRE_BLE_ADDRESS_LEN) == 0);
}

uint32_t TrackerStorage::GetCurrentTimeMs() const {
  return static_cast<uint32_t>(
      (chreGetTime() +
       static_cast<uint64_t>(chreGetEstimatedHostTimeOffset())) /
      chre::kOneMillisecondInNanoseconds);
}

}  // namespace nearby

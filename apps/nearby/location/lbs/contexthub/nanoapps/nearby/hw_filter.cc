#include "location/lbs/contexthub/nanoapps/nearby/hw_filter.h"

#include <cstdint>

#include "chre_api/chre.h"
#include "third_party/contexthub/chre/util/include/chre/util/dynamic_vector.h"

#define LOG_TAG "[NEARBY][HW_FILTER]"

namespace nearby {

bool HwFilter::Match(const chreBleGenericFilter& hardware_filter,
                     const chreBleAdvertisingReport& report) {
  // Scans through data and parses each advertisement structure.
  for (int i = 0; i < report.dataLength;) {
    // First byte has the advertisement data length including type and data.
    uint8_t ad_type_data_length = report.data[i];
    // Early termination with zero length advertisement.
    if (ad_type_data_length == 0) break;
    // Terminates when advertisement length passes over the end of data
    // buffer.
    if (ad_type_data_length >= report.dataLength - i) break;
    // Second byte has advertisement data type.
    ++i;
    // Moves to the next data structure if advertisement data length is less
    // than filter length regardless of data mask or advertisement data
    // type is different from filter type.
    if (ad_type_data_length - 1 >= hardware_filter.len &&
        report.data[i] == hardware_filter.type) {
      // Assumes advertisement data structure is matched.
      bool matched = true;
      // Data should match through data filter mask within filter length.
      for (int j = 0; j < hardware_filter.len; ++j) {
        if ((report.data[i + 1 + j] & hardware_filter.dataMask[j]) !=
            (hardware_filter.data[j] & hardware_filter.dataMask[j])) {
          matched = false;
          break;
        }
      }
      if (matched) {
        return true;
      }
    }
    // Moves to next advertisement structure.
    i += ad_type_data_length;
  }
  return false;
}

bool HwFilter::Match(
    const chre::DynamicVector<chreBleGenericFilter>& hardware_filters,
    const chreBleAdvertisingReport& report) {
  for (const auto hardware_filter : hardware_filters) {
    if (Match(hardware_filter, report)) {
      return true;
    }
  }
  return false;
}

bool HwFilter::CheckRssi(int8_t rssi_threshold,
                         const chreBleAdvertisingReport& report) {
  return (rssi_threshold == CHRE_BLE_RSSI_NONE ||
          (report.rssi != CHRE_BLE_RSSI_NONE && report.rssi >= rssi_threshold));
}

}  // namespace nearby

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

#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/sensor_helpers.h"

#include <memory>

#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

namespace lbs {
namespace contexthub {
namespace helpers {
namespace {

class ChreSensorTrueForTestClass : public ChreApiSensorFunctions {
  bool SensorFindDefault(uint8_t /* sensorType */,
                         uint32_t * /* handle */) override {
    return true;
  }
  bool SensorGetInfo(uint32_t /* sensorHandle */,
                     SensorInfo * /* info */) override {
    return true;
  }
  bool SensorGetSamplingStatus(uint32_t /* sensorHandle */,
                               SamplingStatus * /* status */) override {
    return true;
  }
  bool SensorConfigure(uint32_t /* sensorHandle */,
                       SensorConfigureMode /* mode */, uint64_t /* interval */,
                       uint64_t /* latency */) override {
    return true;
  }
  bool SensorConfigureBiasEvents(uint32_t /* sensorHandle */,
                                 bool /* enable */) override {
    return true;
  }
  bool SensorGetThreeAxisBias(uint32_t /* sensorHandle */,
                              ThreeAxisData * /* bias */) override {
    return true;
  }
  bool SensorFlushAsync(uint32_t /* sensorHandle */,
                        const void * /* cookie */) override {
    return true;
  }
};

TEST_NANOAPP(ChreSensorHelpersTest, SanityTest) {
  EXPECT_CALL(*chre_api_fake_detector_, chreSensorFindDefault).Times(2);
  EXPECT_CALL(*chre_api_fake_detector_, chreGetSensorInfo).Times(2);
  EXPECT_CALL(*chre_api_fake_detector_, chreGetSensorSamplingStatus).Times(2);
  EXPECT_CALL(*chre_api_fake_detector_, chreSensorConfigure).Times(2);
  EXPECT_CALL(*chre_api_fake_detector_, chreSensorConfigureBiasEvents).Times(2);
  EXPECT_CALL(*chre_api_fake_detector_, chreSensorGetThreeAxisBias).Times(2);
  EXPECT_CALL(*chre_api_fake_detector_, chreSensorFlushAsync).Times(2);

  SetEmptySensor();

  EXPECT_FALSE(chreSensorFindDefault(0, nullptr));
  EXPECT_FALSE(chreGetSensorInfo(0, nullptr));
  EXPECT_FALSE(chreGetSensorSamplingStatus(0, nullptr));
  EXPECT_FALSE(
      chreSensorConfigure(0, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 0, 0));
  EXPECT_FALSE(chreSensorConfigureBiasEvents(0, false));
  EXPECT_FALSE(chreSensorGetThreeAxisBias(false, nullptr));
  EXPECT_FALSE(chreSensorFlushAsync(false, nullptr));

  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreSensorTrueForTestClass>());

  EXPECT_TRUE(chreSensorFindDefault(0, nullptr));
  EXPECT_TRUE(chreGetSensorInfo(0, nullptr));
  EXPECT_TRUE(chreGetSensorSamplingStatus(0, nullptr));
  EXPECT_TRUE(
      chreSensorConfigure(0, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 0, 0));
  EXPECT_TRUE(chreSensorConfigureBiasEvents(0, false));
  EXPECT_TRUE(chreSensorGetThreeAxisBias(false, nullptr));
  EXPECT_TRUE(chreSensorFlushAsync(false, nullptr));
}

}  // namespace
}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs
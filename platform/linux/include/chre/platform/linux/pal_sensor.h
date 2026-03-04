/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_LINUX_PAL_SENSOR_H_
#define CHRE_PLATFORM_LINUX_PAL_SENSOR_H_

#include <stdint.h>

/**
 * @param sensorHandle The handle of the sensor to check.
 * @return whether the sensor with the given handle is active.
 */
bool chrePalSensorIsEnabled(uint32_t sensorHandle);

/**
 * Sets whether one-shot sensor events are triggered manually.
 *
 * When manual mode is enabled, the one-shot sensor event is not sent
 * automatically by the PAL upon request. Instead, it must be triggered
 * manually via chrePalSensorSendOneShotSignificantMotionDataEvent(). This is
 * useful for testing scenarios where precise control over event timing is
 * needed.
 *
 * @param enable true to enable manual one-shot event mode, false to disable.
 */
void chrePalSensorSetManualOneShotEventMode(bool enable);

/**
 * Manually sends a one-shot significant motion data event.
 *
 * This function is intended to be used for testing when manual one-shot event
 * mode is enabled.
 *
 * @see chrePalSensorSetManualOneShotEventMode
 */
void chrePalSensorSendOneShotSignificantMotionDataEvent();

#endif  // CHRE_PLATFORM_LINUX_PAL_SENSOR_H_

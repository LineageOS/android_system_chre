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

#include "chre_host/config_util.h"
#include "chre_host/log.h"

#include <android-base/properties.h>
#include <android-base/strings.h>
#include <dirent.h>
#include <json/json.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <unordered_set>

namespace android {
namespace chre {

bool findAllNanoappsInFolder(const std::string &path,
                             std::vector<std::string> &outNanoapps,
                             std::unordered_set<std::string> &nanoappNameSet) {
  DIR *dir = opendir(path.c_str());
  if (dir == nullptr) {
    LOGW("Failed to open nanoapp folder %s, skipped search on this folder.",
         path.c_str());
    return true;
  }
  std::regex regex("(\\w+)\\.napp_header");
  std::cmatch match;
  for (struct dirent *entry; (entry = readdir(dir)) != nullptr;) {
    if (!std::regex_match(entry->d_name, match, regex)) {
      continue;
    }
    std::string nanoapp_name = match[1];
    LOGD("Found nanoapp: %s", nanoapp_name.c_str());
    if (nanoappNameSet.contains(nanoapp_name)) {
      LOGW(
          "Duplicate nanoapp found: '%s' exists in multiple directories "
          "(current dir: %s)",
          nanoapp_name.c_str(), path.c_str());
      // Skips same nanoapps exits in different directories.
      continue;
    }
    nanoappNameSet.insert(nanoapp_name);
    outNanoapps.push_back(nanoapp_name);
  }
  closedir(dir);
  std::sort(outNanoapps.begin(), outNanoapps.end());
  return true;
}

bool getPreloadedNanoapps(
    const std::string &systemNanoappPath,
    std::unordered_map<std::string, std::vector<std::string>>
        &outNanoappsByDir) {
  std::unordered_set<std::string> nanoappNameSet;

  // Get nanoapps from path set in the system property
  // Scan the path in system property first, so that nanoapps with the same name
  // in this directory will override the system nanoapps.
  std::string propertyPath =
      android::base::GetProperty("vendor.chre.preloaded_nanoapps.path", "");
  if (!propertyPath.empty()) {
    LOGI(
        "Get additional preloaded nanoapps from system property "
        "vendor.chre.preloaded_nanoapps.path: %s",
        propertyPath.c_str());
    std::vector<std::string> paths = android::base::Split(propertyPath, ",");
    for (const auto &path : paths) {
      std::vector<std::string> additionalNanoapps;
      if (!findAllNanoappsInFolder(path, additionalNanoapps, nanoappNameSet)) {
        continue;
      }
      if (!additionalNanoapps.empty()) {
        outNanoappsByDir[path] = std::move(additionalNanoapps);
      }
    }
  }

  // Get nanoapps from system nanoapp path
  LOGI("Get preloaded nanoapps from system nanoapp path: %s",
       systemNanoappPath.c_str());
  std::filesystem::path path(systemNanoappPath);
  std::vector<std::string> systemNanoapps;
  if (!findAllNanoappsInFolder(systemNanoappPath, systemNanoapps,
                               nanoappNameSet)) {
    return false;
  }
  if (!systemNanoapps.empty()) {
    outNanoappsByDir[systemNanoappPath] = std::move(systemNanoapps);
  }
  return true;
}

bool getPreloadedNanoappsFromConfigFile(
    const std::string &configFilePath,
    std::unordered_map<std::string, std::vector<std::string>>
        &outNanoappsByDir) {
  std::ifstream configFileStream(configFilePath);

  Json::CharReaderBuilder builder;
  Json::Value config;
  if (!configFileStream) {
    // TODO(b/350102369) to deprecate preloaded_nanoapps.json
    // During the transition, fall back to the old behavior if the json
    // file exists. But if the json file does not exist, do the new behavior
    // to load all nanoapps in /vendor/etc/chre or where ever the location.
    LOGI("Failed to open config file '%s' load all nanoapps in folder ",
         configFilePath.c_str());
    std::filesystem::path path(configFilePath);
    const std::string systemNanoappPath = path.parent_path().string();
    return getPreloadedNanoapps(systemNanoappPath, outNanoappsByDir);
  } else if (!Json::parseFromStream(builder, configFileStream, &config,
                                    /* errs = */ nullptr)) {
    LOGE("Failed to parse nanoapp config file");
    return false;
  } else if (!config.isMember("nanoapps") || !config.isMember("source_dir")) {
    LOGE("Malformed preloaded nanoapps config");
    return false;
  }

  const std::string outDirectory = config["source_dir"].asString();
  std::vector<std::string> outNanoapps;
  for (Json::ArrayIndex i = 0; i < config["nanoapps"].size(); ++i) {
    const std::string &nanoappName = config["nanoapps"][i].asString();
    outNanoapps.push_back(nanoappName);
  }
  outNanoappsByDir[outDirectory] = std::move(outNanoapps);
  return true;
}

}  // namespace chre
}  // namespace android

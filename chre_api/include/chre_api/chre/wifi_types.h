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

#ifndef CHRE_WIFI_TYPES_H_
#define CHRE_WIFI_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The venue group of the access point, as defined by the 802.11 specification.
 * Refer to IEEE 802.11-2024 for more information.
 * 9.4.1.32 describes the Venue Info field. (Venue Group and Venue Type).
 * 9.4.2.90 describes the overarching Interworking Element.
 *
 * The enum values are guaranteed to fit within a uint8_t.
 */
enum chreWifiVenueGroup {
  CHRE_WIFI_VENUE_GROUP_UNSPECIFIED               = 0,
  CHRE_WIFI_VENUE_GROUP_ASSEMBLY                  = 1,
  CHRE_WIFI_VENUE_GROUP_BUSINESS                  = 2,
  CHRE_WIFI_VENUE_GROUP_EDUCATIONAL               = 3,
  CHRE_WIFI_VENUE_GROUP_FACTORY_AND_INDUSTRIAL    = 4,
  CHRE_WIFI_VENUE_GROUP_INSTITUTIONAL             = 5,
  CHRE_WIFI_VENUE_GROUP_MERCANTILE                = 6,
  CHRE_WIFI_VENUE_GROUP_RESIDENTIAL               = 7,
  CHRE_WIFI_VENUE_GROUP_STORAGE                   = 8,
  CHRE_WIFI_VENUE_GROUP_UTILITY_AND_MISCELLANEOUS = 9,
  CHRE_WIFI_VENUE_GROUP_VEHICULAR                 = 10,
  CHRE_WIFI_VENUE_GROUP_OUTDOOR                   = 11,
  CHRE_WIFI_VENUE_GROUP_CREDENTIAL                = 12,
};

/**
 * The full venue info (group + type) of the access point
 * Refer to IEEE 802.11-2024 for more information.
 * 9.4.1.32 describes the Venue Info field. (Venue Group and Venue Type).
 * 9.4.2.90 describes the overarching Interworking Element.
 *
 * The enum values are guaranteed to fit within a uint16_t. The format is:
 * (Venue Group << 8) | Venue Type.
 */
enum chreWifiVenueInfo {
  CHRE_WIFI_VENUE_UNSPECIFIED_UNSPECIFIED                    =  (0 << 8) |  0,
  CHRE_WIFI_VENUE_ASSEMBLY_UNSPECIFIED                       =  (1 << 8) |  0,
  CHRE_WIFI_VENUE_ASSEMBLY_ARENA                             =  (1 << 8) |  1,
  CHRE_WIFI_VENUE_ASSEMBLY_STADIUM                           =  (1 << 8) |  2,
  CHRE_WIFI_VENUE_ASSEMBLY_PASSENGER_TERMINAL                =  (1 << 8) |  3,
  CHRE_WIFI_VENUE_ASSEMBLY_AMPHITHEATER                      =  (1 << 8) |  4,
  CHRE_WIFI_VENUE_ASSEMBLY_AMUSEMENT_PARK                    =  (1 << 8) |  5,
  CHRE_WIFI_VENUE_ASSEMBLY_PLACE_OF_WORSHIP                  =  (1 << 8) |  6,
  CHRE_WIFI_VENUE_ASSEMBLY_CONVENTION_CENTER                 =  (1 << 8) |  7,
  CHRE_WIFI_VENUE_ASSEMBLY_LIBRARY                           =  (1 << 8) |  8,
  CHRE_WIFI_VENUE_ASSEMBLY_MUSEUM                            =  (1 << 8) |  9,
  CHRE_WIFI_VENUE_ASSEMBLY_RESTAURANT                        =  (1 << 8) | 10,
  CHRE_WIFI_VENUE_ASSEMBLY_THEATER                           =  (1 << 8) | 11,
  CHRE_WIFI_VENUE_ASSEMBLY_BAR                               =  (1 << 8) | 12,
  CHRE_WIFI_VENUE_ASSEMBLY_COFFEE_SHOP                       =  (1 << 8) | 13,
  CHRE_WIFI_VENUE_ASSEMBLY_ZOO_OR_AQUARIUM                   =  (1 << 8) | 14,
  CHRE_WIFI_VENUE_ASSEMBLY_EMERGENCY_COORDINATION_CENTER     =  (1 << 8) | 15,
  CHRE_WIFI_VENUE_BUSINESS_UNSPECIFIED                       =  (2 << 8) |  0,
  CHRE_WIFI_VENUE_BUSINESS_DOCTOR_OR_DENTIST_OFFICE          =  (2 << 8) |  1,
  CHRE_WIFI_VENUE_BUSINESS_BANK                              =  (2 << 8) |  2,
  CHRE_WIFI_VENUE_BUSINESS_FIRE_STATION                      =  (2 << 8) |  3,
  CHRE_WIFI_VENUE_BUSINESS_POLICE_STATION                    =  (2 << 8) |  4,
  CHRE_WIFI_VENUE_BUSINESS_POST_OFFICE                       =  (2 << 8) |  6,
  CHRE_WIFI_VENUE_BUSINESS_PROFESSIONAL_OFFICE               =  (2 << 8) |  7,
  CHRE_WIFI_VENUE_BUSINESS_RESEARCH_AND_DEVELOPMENT_FACILITY =  (2 << 8) |  8,
  CHRE_WIFI_VENUE_BUSINESS_ATTORNEY_OFFICE                   =  (2 << 8) |  9,
  CHRE_WIFI_VENUE_EDUCATIONAL_UNSPECIFIED                    =  (3 << 8) |  0,
  CHRE_WIFI_VENUE_EDUCATIONAL_SCHOOL_PRIMARY                 =  (3 << 8) |  1,
  CHRE_WIFI_VENUE_EDUCATIONAL_SCHOOL_SECONDARY               =  (3 << 8) |  2,
  CHRE_WIFI_VENUE_EDUCATIONAL_UNIVERSITY_OR_COLLEGE          =  (3 << 8) |  3,
  CHRE_WIFI_VENUE_FACTORY_AND_INDUSTRIAL_UNSPECIFIED         =  (4 << 8) |  0,
  CHRE_WIFI_VENUE_FACTORY_AND_INDUSTRIAL_FACTORY             =  (4 << 8) |  1,
  CHRE_WIFI_VENUE_INSTITUTIONAL_UNSPECIFIED                  =  (5 << 8) |  0,
  CHRE_WIFI_VENUE_INSTITUTIONAL_HOSPITAL                     =  (5 << 8) |  1,
  CHRE_WIFI_VENUE_INSTITUTIONAL_LONG_TERM_CARE_FACILITY      =  (5 << 8) |  2,
  CHRE_WIFI_VENUE_INSTITUTIONAL_ALCOHOL_AND_DRUG_REHABILITATION_CENTER\
                                                             =  (5 << 8) |  3,
  CHRE_WIFI_VENUE_INSTITUTIONAL_GROUP_HOME                   =  (5 << 8) |  4,
  CHRE_WIFI_VENUE_INSTITUTIONAL_PRISON_OR_JAIL               =  (5 << 8) |  5,
  CHRE_WIFI_VENUE_MERCANTILE_UNSPECIFIED                     =  (6 << 8) |  0,
  CHRE_WIFI_VENUE_MERCANTILE_RETAIL_STORE                    =  (6 << 8) |  1,
  CHRE_WIFI_VENUE_MERCANTILE_GROCERY_MARKET                  =  (6 << 8) |  2,
  CHRE_WIFI_VENUE_MERCANTILE_AUTOMOTIVE_SERVICE_STATION      =  (6 << 8) |  3,
  CHRE_WIFI_VENUE_MERCANTILE_SHOPPING_MALL                   =  (6 << 8) |  4,
  CHRE_WIFI_VENUE_MERCANTILE_GAS_STATION                     =  (6 << 8) |  5,
  CHRE_WIFI_VENUE_RESIDENTIAL_UNSPECIFIED                    =  (7 << 8) |  0,
  CHRE_WIFI_VENUE_RESIDENTIAL_PRIVATE_RESIDENCE              =  (7 << 8) |  1,
  CHRE_WIFI_VENUE_RESIDENTIAL_HOTEL_OR_MOTEL                 =  (7 << 8) |  2,
  CHRE_WIFI_VENUE_RESIDENTIAL_DORMITORY                      =  (7 << 8) |  3,
  CHRE_WIFI_VENUE_RESIDENTIAL_BOARDING_HOUSE                 =  (7 << 8) |  4,
  CHRE_WIFI_VENUE_STORAGE_UNSPECIFIED                        =  (8 << 8) |  0,
  CHRE_WIFI_VENUE_UTILITY_AND_MISCELLANEOUS_UNSPECIFIED      =  (9 << 8) |  0,
  CHRE_WIFI_VENUE_VEHICULAR_UNSPECIFIED                      = (10 << 8) |  0,
  CHRE_WIFI_VENUE_VEHICULAR_AUTOMOBILE_OR_TRUCK              = (10 << 8) |  1,
  CHRE_WIFI_VENUE_VEHICULAR_AIRPLANE                         = (10 << 8) |  2,
  CHRE_WIFI_VENUE_VEHICULAR_BUS                              = (10 << 8) |  3,
  CHRE_WIFI_VENUE_VEHICULAR_FERRY                            = (10 << 8) |  4,
  CHRE_WIFI_VENUE_VEHICULAR_SHIP_OR_BOAT                     = (10 << 8) |  5,
  CHRE_WIFI_VENUE_VEHICULAR_TRAIN                            = (10 << 8) |  6,
  CHRE_WIFI_VENUE_VEHICULAR_MOTOR_BIKE                       = (10 << 8) |  7,
  CHRE_WIFI_VENUE_OUTDOOR_UNSPECIFIED                        = (11 << 8) |  0,
  CHRE_WIFI_VENUE_OUTDOOR_MUNI_MESH_NETWORK                  = (11 << 8) |  1,
  CHRE_WIFI_VENUE_OUTDOOR_CITY_PARK                          = (11 << 8) |  2,
  CHRE_WIFI_VENUE_OUTDOOR_REST_AREA                          = (11 << 8) |  3,
  CHRE_WIFI_VENUE_OUTDOOR_TRAFFIC_CONTROL                    = (11 << 8) |  4,
  CHRE_WIFI_VENUE_OUTDOOR_BUS_STOP                           = (11 << 8) |  5,
  CHRE_WIFI_VENUE_OUTDOOR_KIOSK                              = (11 << 8) |  6,
  CHRE_WIFI_VENUE_CREDENTIAL_UNSPECIFIED                     = (12 << 8) |  0,
  CHRE_WIFI_VENUE_CREDENTIAL_SERVICE_PROVIDER                = (12 << 8) |  1,
  CHRE_WIFI_VENUE_CREDENTIAL_CLOUD_PROVIDER                  = (12 << 8) |  2,
  CHRE_WIFI_VENUE_CREDENTIAL_CABLE_INDUSTRY                  = (12 << 8) |  3,
  CHRE_WIFI_VENUE_CREDENTIAL_GOVERNMENT                      = (12 << 8) |  4,
  CHRE_WIFI_VENUE_CREDENTIAL_SOCIAL_MEDIA_PROVIDER           = (12 << 8) |  5,
};

#ifdef __cplusplus
}
#endif

#endif  // CHRE_WIFI_TYPES_H_
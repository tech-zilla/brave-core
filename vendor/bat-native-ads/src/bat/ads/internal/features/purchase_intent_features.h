/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_FEATURES_PURCHASE_INTENT_FEATURES_H_
#define BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_FEATURES_PURCHASE_INTENT_FEATURES_H_

#include <cstdint>

#include "base/feature_list.h"  // IWYU pragma: keep

namespace base {
class TimeDelta;
}  // namespace base

namespace ads::targeting::features {

extern const base::Feature kPurchaseIntent;

bool IsPurchaseIntentEnabled();

uint16_t GetPurchaseIntentThreshold();

base::TimeDelta GetPurchaseIntentTimeWindow();

int GetPurchaseIntentResourceVersion();

}  // namespace ads::targeting::features

#endif  // BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_FEATURES_PURCHASE_INTENT_FEATURES_H_

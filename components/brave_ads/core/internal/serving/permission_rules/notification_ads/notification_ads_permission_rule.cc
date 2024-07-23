/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/serving/permission_rules/notification_ads/notification_ads_permission_rule.h"

#include "brave/components/brave_ads/core/internal/client/ads_client_util.h"
#include "brave/components/brave_ads/core/internal/common/logging_util.h"

namespace brave_ads {

bool HasNotificationAdsPermission() {
  if (CanShowNotificationAds()) {
    return true;
  }

  BLOG(2, "System notifications not allowed");
  return false;
}

}  // namespace brave_ads

/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <utility>

#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "bat/ledger/global_constants.h"
#include "bat/ledger/internal/common/time_util.h"
#include "bat/ledger/internal/endpoint/uphold/uphold_server.h"
#include "bat/ledger/internal/ledger_impl.h"
#include "bat/ledger/internal/logging/event_log_keys.h"
#include "bat/ledger/internal/notifications/notification_keys.h"
#include "bat/ledger/internal/state/state_keys.h"
#include "bat/ledger/internal/uphold/uphold.h"
#include "bat/ledger/internal/uphold/uphold_authorization.h"
#include "bat/ledger/internal/uphold/uphold_card.h"
#include "bat/ledger/internal/uphold/uphold_transfer.h"
#include "bat/ledger/internal/uphold/uphold_util.h"
#include "bat/ledger/internal/uphold/uphold_wallet.h"
#include "bat/ledger/internal/wallet/wallet_util.h"
#include "brave_base/random.h"

using ledger::wallet::OnWalletStatusChange;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

namespace {
const char kFeeMessage[] =
    "5% transaction fee collected by Brave Software International";
}  // namespace

namespace ledger {
namespace uphold {

Uphold::Uphold(LedgerImpl* ledger)
    : transfer_(std::make_unique<UpholdTransfer>(ledger)),
      card_(std::make_unique<UpholdCard>(ledger)),
      user_(std::make_unique<UpholdUser>(ledger)),
      authorization_(std::make_unique<UpholdAuthorization>(ledger)),
      wallet_(std::make_unique<UpholdWallet>(ledger)),
      uphold_server_(std::make_unique<endpoint::UpholdServer>(ledger)),
      ledger_(ledger) {}

Uphold::~Uphold() = default;

void Uphold::Initialize() {
  auto wallet = GetWallet();
  if (!wallet) {
    return;
  }

  for (auto const& value : wallet->fees) {
    StartTransferFeeTimer(value.first, 1);
  }
}

void Uphold::StartContribution(const std::string& contribution_id,
                               mojom::ServerPublisherInfoPtr info,
                               double amount,
                               ledger::LegacyResultCallback callback) {
  if (!info) {
    BLOG(0, "Publisher info is null");
    ContributionCompleted(mojom::Result::LEDGER_ERROR, "", contribution_id,
                          amount, "", callback);
    return;
  }

  const double fee = (amount * 1.05) - amount;
  const double reconcile_amount = amount - fee;

  auto contribution_callback =
      std::bind(&Uphold::ContributionCompleted, this, _1, _2, contribution_id,
                fee, info->publisher_key, callback);

  Transaction transaction;
  transaction.address = info->address;
  transaction.amount = reconcile_amount;

  transfer_->Start(transaction, contribution_callback);
}

void Uphold::ContributionCompleted(mojom::Result result,
                                   const std::string& transaction_id,
                                   const std::string& contribution_id,
                                   double fee,
                                   const std::string& publisher_key,
                                   ledger::LegacyResultCallback callback) {
  if (result == mojom::Result::LEDGER_OK) {
    SaveTransferFee(contribution_id, fee);

    if (!publisher_key.empty()) {
      ledger_->database()->UpdateContributionInfoContributedAmount(
          contribution_id, publisher_key, callback);
      return;
    }
  }

  callback(result);
}

void Uphold::FetchBalance(FetchBalanceCallback callback) {
  auto uphold_wallet = GetWallet();
  if (!uphold_wallet) {
    BLOG(1, "Uphold wallet is null.");
    return std::move(callback).Run(mojom::Result::LEDGER_OK, 0.0);
  }

  if (uphold_wallet->status != mojom::WalletStatus::VERIFIED) {
    BLOG(1, "Uphold wallet is not VERIFIED.");
    return std::move(callback).Run(mojom::Result::LEDGER_OK, 0.0);
  }

  CheckWalletState(uphold_wallet.get());

  auto url_callback = base::BindOnce(
      &Uphold::OnFetchBalance, base::Unretained(this), std::move(callback));

  uphold_server_->get_card()->Request(
      uphold_wallet->address, uphold_wallet->token, std::move(url_callback));
}

void Uphold::OnFetchBalance(FetchBalanceCallback callback,
                            const mojom::Result result,
                            const double available) {
  auto uphold_wallet = GetWallet();
  if (!uphold_wallet) {
    BLOG(0, "Uphold wallet is null!");
    return std::move(callback).Run(mojom::Result::LEDGER_ERROR, 0.0);
  }

  if (uphold_wallet->status != mojom::WalletStatus::VERIFIED) {
    BLOG(0, "Wallet status should have been VERIFIED!");
    return std::move(callback).Run(mojom::Result::LEDGER_ERROR, 0.0);
  }

  CheckWalletState(uphold_wallet.get());

  if (result == mojom::Result::EXPIRED_TOKEN) {
    BLOG(0, "Expired token");
    DisconnectWallet(ledger::notifications::kWalletDisconnected);
    return std::move(callback).Run(mojom::Result::EXPIRED_TOKEN, 0.0);
  }

  if (result != mojom::Result::LEDGER_OK) {
    BLOG(0, "Couldn't get balance");
    return std::move(callback).Run(mojom::Result::LEDGER_ERROR, 0.0);
  }

  std::move(callback).Run(mojom::Result::LEDGER_OK, available);
}

void Uphold::TransferFunds(const double amount,
                           const std::string& address,
                           client::TransactionCallback callback) {
  Transaction transaction;
  transaction.address = address;
  transaction.amount = amount;
  transfer_->Start(transaction, callback);
}

void Uphold::WalletAuthorization(
    const base::flat_map<std::string, std::string>& args,
    ledger::ExternalWalletAuthorizationCallback callback) {
  authorization_->Authorize(args, callback);
}

void Uphold::GenerateWallet(ledger::ResultCallback callback) {
  wallet_->Generate(std::move(callback));
}

void Uphold::CreateCard(CreateCardCallback callback) {
  card_->CreateBATCardIfNecessary(std::move(callback));
}

void Uphold::DisconnectWallet(const absl::optional<std::string>& notification) {
  auto wallet = GetWallet();
  if (!wallet) {
    return;
  }

  BLOG(1, "Disconnecting wallet");
  const std::string wallet_address = wallet->address;

  const bool manual = !notification.has_value();

  const auto from = wallet->status;
  wallet = ledger::wallet::ResetWallet(std::move(wallet));
  if (manual) {
    wallet->status = mojom::WalletStatus::NOT_CONNECTED;
  }
  const auto to = wallet->status;

  OnWalletStatusChange(ledger_, from, to);

  const bool shutting_down = ledger_->IsShuttingDown();

  if (!manual && !shutting_down && !notification->empty()) {
    ledger_->ledger_client()->ShowNotification(*notification, {"Uphold"},
                                               [](mojom::Result) {});
  }

  wallet = GenerateLinks(std::move(wallet));
  SetWallet(std::move(wallet));

  if (!shutting_down) {
    ledger_->ledger_client()->WalletDisconnected(constant::kWalletUphold);
  }

  ledger_->database()->SaveEventLog(log::kWalletDisconnected,
                                    std::string(constant::kWalletUphold) +
                                        (!wallet_address.empty() ? "/" : "") +
                                        wallet_address.substr(0, 5));
}

void Uphold::GetUser(GetUserCallback callback) {
  user_->Get(std::move(callback));
}

void Uphold::GetCapabilities(GetCapabilitiesCallback callback) {
  auto uphold_wallet = GetWallet();
  if (!uphold_wallet) {
    BLOG(0, "Uphold wallet is null!");
    return std::move(callback).Run(mojom::Result::LEDGER_ERROR, {});
  }

  if (uphold_wallet->status != mojom::WalletStatus::PENDING &&
      uphold_wallet->status != mojom::WalletStatus::VERIFIED) {
    BLOG(0, "Uphold wallet is neither in PENDING, nor in VERIFIED state!");
    return std::move(callback).Run(mojom::Result::LEDGER_ERROR, {});
  }

  CheckWalletState(uphold_wallet.get());

  uphold_server_->get_capabilities()->Request(uphold_wallet->token,
                                              std::move(callback));
}

void Uphold::SaveTransferFee(const std::string& contribution_id,
                             const double fee) {
  StartTransferFeeTimer(contribution_id, 1);

  auto wallet = GetWallet();
  if (!wallet) {
    BLOG(0, "Wallet is null");
    return;
  }

  wallet->fees.insert(std::make_pair(contribution_id, fee));
  SetWallet(std::move(wallet));
}

void Uphold::StartTransferFeeTimer(const std::string& fee_id, int attempts) {
  DCHECK(!fee_id.empty());

  base::TimeDelta delay = util::GetRandomizedDelay(base::Seconds(45));

  BLOG(1, "Uphold transfer fee timer set for " << delay);

  transfer_fee_timers_[fee_id].Start(
      FROM_HERE, delay,
      base::BindOnce(&Uphold::OnTransferFeeTimerElapsed, base::Unretained(this),
                     fee_id, attempts));
}

void Uphold::OnTransferFeeCompleted(const mojom::Result result,
                                    const std::string& transaction_id,
                                    const std::string& contribution_id,
                                    int attempts) {
  if (result != mojom::Result::LEDGER_OK) {
    if (attempts < 3) {
      BLOG(0, "Transaction fee failed, retrying");
      StartTransferFeeTimer(contribution_id, attempts + 1);
      return;
    }
    BLOG(0, "Transaction fee failed, no remaining attempts this session");
    return;
  }

  RemoveTransferFee(contribution_id);
}

void Uphold::TransferFee(const std::string& contribution_id,
                         const double amount,
                         int attempts) {
  auto transfer_callback = std::bind(&Uphold::OnTransferFeeCompleted, this, _1,
                                     _2, contribution_id, attempts);

  Transaction transaction;
  transaction.address = GetFeeAddress();
  transaction.amount = amount;
  transaction.message = kFeeMessage;

  transfer_->Start(transaction, transfer_callback);
}

void Uphold::OnTransferFeeTimerElapsed(const std::string& id, int attempts) {
  transfer_fee_timers_.erase(id);

  auto wallet = GetWallet();
  if (!wallet) {
    BLOG(0, "Wallet is null");
    return;
  }

  for (const auto& value : wallet->fees) {
    if (value.first == id) {
      TransferFee(value.first, value.second, attempts);
      return;
    }
  }
}

mojom::ExternalWalletPtr Uphold::GetWallet() {
  return ::ledger::wallet::GetWallet(ledger_, constant::kWalletUphold);
}

bool Uphold::SetWallet(mojom::ExternalWalletPtr wallet) {
  return ::ledger::wallet::SetWallet(ledger_, std::move(wallet),
                                     state::kWalletUphold);
}

void Uphold::RemoveTransferFee(const std::string& contribution_id) {
  auto wallet = GetWallet();
  if (!wallet) {
    BLOG(0, "Wallet is null");
    return;
  }

  wallet->fees.erase(contribution_id);
  SetWallet(std::move(wallet));
}

}  // namespace uphold
}  // namespace ledger

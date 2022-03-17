// Concord
//
// Copyright (c) 2018-2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.
// This module creates an instance of ClientImp class using input
// parameters and launches a bunch of tests created by TestsBuilder towards
// concord::consensus::ReplicaImp objects.

#include "app_state.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>

void Account::depositPublic(int val) { publicBalance_ += val; }

int Account::withdrawPublic(int val) {
  val = std::min<int>(publicBalance_, val);
  publicBalance_ -= val;
  return val;
}

std::ostream& operator<<(std::ostream& os, const Block& b) {
  os << b.id_ << " | ";
  b.tx_ ? os << *b.tx_ : os << "(Empty)";
  return os;
}

AppState::AppState() {
  accounts_.emplace("A", Account("A"));
  accounts_.emplace("B", Account("B"));
  accounts_.emplace("C", Account("C"));
  blocks_.emplace_back();  // Genesis block
}

void AppState::setLastKnownBlockId(BlockId id) { lastKnownBlockId_ = std::max<int>(lastKnownBlockId_, id); }

BlockId AppState::getLastKnowBlockId() const { return lastKnownBlockId_; }

const Block* AppState::getBlockById(BlockId id) const {
  if (id >= blocks_.size()) return nullptr;
  return &blocks_[id];
}

std::optional<BlockId> AppState::sync() {
  for (int i = lastExecutedBlockId_ + 1; i < (int)blocks_.size(); ++i)
    if (blocks_[i].tx_) executeTx(*blocks_[i].tx_);

  lastExecutedBlockId_ = blocks_.size() - 1;

  return lastExecutedBlockId_ < lastKnownBlockId_ ? std::optional<int>(lastExecutedBlockId_ + 1) : std::nullopt;
}

BlockId AppState::appendBlock(Block&& bl) {
  BlockId nextBlockId = blocks_.size();
  bl.id_ = nextBlockId;
  blocks_.emplace_back(std::move(bl));
  lastKnownBlockId_ = std::max<int>(lastKnownBlockId_, nextBlockId);
  return nextBlockId;
}

const std::map<std::string, Account> AppState::GetAccounts() const { return accounts_; }

const std::vector<Block>& AppState::GetBlocks() const { return blocks_; }

const Account* AppState::getAccountById(const std::string& id) const {
  auto it = accounts_.find(id);
  return it != accounts_.end() ? &(it->second) : nullptr;
}

Account* AppState::getAccountById(const std::string& id) {
  auto it = accounts_.find(id);
  return it != accounts_.end() ? &(it->second) : nullptr;
}

void AppState::validateTx(const Tx& tx) const {
  struct Visitor {
    const AppState& state_;

    Visitor(const AppState& state) : state_(state) {}

    void operator()(const TxPublicDeposit& tx) const {
      if (tx.amount_ <= 0) throw std::domain_error("Deposit amount must be positive!");
      auto acc = state_.getAccountById(tx.toAccountId_);
      if (!acc) throw std::domain_error("Unknown account for public deposit!");
    }

    void operator()(const TxPublicWithdraw& tx) const {
      if (tx.amount_ <= 0) throw std::domain_error("Deposit amount must be positive!");
      auto acc = state_.getAccountById(tx.toAccountId_);
      if (!acc) throw std::domain_error("Unknown account for public withdraw!");
      if (tx.amount_ > acc->getBalancePublic()) throw std::domain_error("Account has insufficient public balance!");
    }

    void operator()(const TxPublicTransfer& tx) const {
      if (tx.amount_ <= 0) throw std::domain_error("Deposit amount must be positive!");
      auto sender = state_.getAccountById(tx.fromAccountId_);
      if (!sender) throw std::domain_error("Unknown sender account for public transfer!");
      auto receiver = state_.getAccountById(tx.toAccountId_);
      if (!receiver) throw std::domain_error("Unknown receiver account for public transfer!");
      if (tx.amount_ > sender->getBalancePublic()) throw std::domain_error("Sender has insufficient public balance!");
    }

    void operator()(const TxUttTransfer& tx) const { /*TODO*/
    }
  };

  std::visit(Visitor{*this}, tx);
}

void AppState::executeTx(const Tx& tx) {
  struct Visitor {
    AppState& state_;

    Visitor(AppState& state) : state_{state} {}

    void operator()(const TxPublicDeposit& tx) {
      auto acc = state_.getAccountById(tx.toAccountId_);
      if (acc) acc->depositPublic(tx.amount_);
    }

    void operator()(const TxPublicWithdraw& tx) {
      auto acc = state_.getAccountById(tx.toAccountId_);
      if (acc) acc->withdrawPublic(tx.amount_);
    }

    void operator()(const TxPublicTransfer& tx) {
      auto sender = state_.getAccountById(tx.fromAccountId_);
      auto receiver = state_.getAccountById(tx.toAccountId_);
      if (sender) sender->withdrawPublic(tx.amount_);
      if (receiver) receiver->depositPublic(tx.amount_);
    }

    void operator()(const TxUttTransfer& tx) {
      // To-Do
    }
  };

  std::visit(Visitor{*this}, tx);
}

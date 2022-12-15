// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").  You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include "SigManager.hpp"
#include "assertUtils.hpp"
#include "ReplicasInfo.hpp"

#include <algorithm>
#include "keys_and_signatures.cmf.hpp"
#include "ReplicaConfig.hpp"
#include "hex_tools.h"
#include "crypto/factory.hpp"
#include "CryptoManager.hpp"
#include "threshsign/eddsa/EdDSAMultisigVerifier.h"
#include "threshsign/eddsa/EdDSAMultisigSigner.h"

using namespace std;

namespace bftEngine {
namespace impl {

using concord::crypto::IVerifier;
using concord::crypto::Factory;
using concord::crypto::KeyFormat;

concord::messages::keys_and_signatures::ClientsPublicKeys clientsPublicKeys_;

std::shared_ptr<SigManager> SigManager::s_sm;

std::string SigManager::getClientsPublicKeys() {
  std::shared_lock lock(mutex_);
  std::vector<uint8_t> output;
  concord::messages::keys_and_signatures::serialize(output, clientsPublicKeys_);
  return std::string(output.begin(), output.end());
}

SigManager* SigManager::instance() {
  ConcordAssertNE(s_sm.get(), nullptr);
  return s_sm.get();
}

void SigManager::reset(std::shared_ptr<SigManager> other) {
  s_sm = other;
}

std::shared_ptr<SigManager> SigManager::init(ReplicaId myId,
                             const Key& mySigPrivateKey,
                             const std::set<std::pair<PrincipalId, const std::string>>& publicKeysOfReplicas,
                             KeyFormat replicasKeysFormat,
                             const std::set<std::pair<const std::string, std::set<uint16_t>>>* publicKeysOfClients,
                             KeyFormat clientsKeysFormat,
                             const ReplicasInfo& replicasInfo) {
  vector<pair<Key, KeyFormat>> publickeys;
  map<PrincipalId, SigManager::KeyIndex> publicKeysMapping;
  size_t lowBound, highBound;
  auto numReplicas = replicasInfo.getNumberOfReplicas();
  auto numRoReplicas = replicasInfo.getNumberOfRoReplicas();
  auto numOfClientProxies = replicasInfo.getNumOfClientProxies();
  auto numOfExternalClients = replicasInfo.getNumberOfExternalClients();
  auto numOfInternalClients = replicasInfo.getNumberOfInternalClients();
  auto numOfClientServices = replicasInfo.getNumberOfClientServices();

  LOG_INFO(
      GL,
      "Compute publicKeysMapping and publickeys: " << KVLOG(
          myId, numReplicas, numRoReplicas, numOfClientProxies, numOfExternalClients, publicKeysOfReplicas.size()));

  SigManager::KeyIndex i{0};
  highBound = numReplicas + numRoReplicas - 1;
  for (const auto& repIdToKeyPair : publicKeysOfReplicas) {
    // each replica sign with a unique private key (1 to 1 relation)
    ConcordAssert(repIdToKeyPair.first <= highBound);
    publickeys.push_back(make_pair(repIdToKeyPair.second, replicasKeysFormat));
    publicKeysMapping.insert({repIdToKeyPair.first, i++});
  }

  if (publicKeysOfClients) {
    // Multiple clients might be signing with the same private key (1 to many relation)
    // Also, we do not enforce to have all range between [lowBound, highBound] constructed. We might want to have less
    // principal ids mapped to keys than what is stated in the range.
    lowBound = numRoReplicas + numReplicas + numOfClientProxies;
    highBound = lowBound + numOfExternalClients + numOfInternalClients + numOfClientServices - 1;
    for (const auto& p : (*publicKeysOfClients)) {
      ConcordAssert(!p.first.empty());
      publickeys.push_back(make_pair(p.first, clientsKeysFormat));
      for (const auto e : p.second) {
        if ((e < lowBound) || (e > highBound)) {
          LOG_FATAL(GL, "Invalid participant id " << KVLOG(e, lowBound, highBound));
          std::terminate();
        }
        publicKeysMapping.insert({e, i});
      }
      ++i;
    }
  }

  LOG_INFO(GL, "Done Compute Start ctor for SigManager with " << KVLOG(publickeys.size(), publicKeysMapping.size()));
  auto ret = std::shared_ptr<SigManager>{new SigManager(
      myId,
      make_pair(mySigPrivateKey, replicasKeysFormat),
      publickeys,
      publicKeysMapping,
      ((ReplicaConfig::instance().clientTransactionSigningEnabled) && (publicKeysOfClients != nullptr)),
      replicasInfo)};

  reset(ret);
  return ret;
}

SigManager::SigManager(PrincipalId myId,
                       const pair<Key, KeyFormat>& mySigPrivateKey,
                       const vector<pair<Key, KeyFormat>>& publickeys,
                       const map<PrincipalId, KeyIndex>& publicKeysMapping,
                       bool clientTransactionSigningEnabled,
                       const ReplicasInfo& replicasInfo)
    : myId_(myId),
      clientTransactionSigningEnabled_(clientTransactionSigningEnabled),
      replicasInfo_(replicasInfo),

      metrics_component_{
          concordMetrics::Component("signature_manager", std::make_shared<concordMetrics::Aggregator>())},

      metrics_{
          metrics_component_.RegisterAtomicCounter("external_client_request_signature_verification_failed"),
          metrics_component_.RegisterAtomicCounter("external_client_request_signatures_verified"),
          metrics_component_.RegisterAtomicCounter("peer_replicas_signature_verification_failed"),
          metrics_component_.RegisterAtomicCounter("peer_replicas_signatures_verified"),
          metrics_component_.RegisterAtomicCounter("signature_verification_failed_on_unrecognized_participant_id")} {
  map<KeyIndex, std::shared_ptr<IVerifier>> publicKeyIndexToVerifier;
  size_t numPublickeys = publickeys.size();

  ConcordAssert(publicKeysMapping.size() >= numPublickeys);
  if (!mySigPrivateKey.first.empty()) {
    mySigner_ = Factory::getSigner(
        mySigPrivateKey.first, ReplicaConfig::instance().replicaMsgSigningAlgo, mySigPrivateKey.second);
  }
  for (const auto& p : publicKeysMapping) {
    ConcordAssert(verifiers_.count(p.first) == 0);
    ConcordAssert(p.second < numPublickeys);

    auto iter = publicKeyIndexToVerifier.find(p.second);
    const auto& [key, format] = publickeys[p.second];
    if (iter == publicKeyIndexToVerifier.end()) {
      verifiers_[p.first] = std::shared_ptr<IVerifier>(
          Factory::getVerifier(key, ReplicaConfig::instance().replicaMsgSigningAlgo, format));
      publicKeyIndexToVerifier[p.second] = verifiers_[p.first];
    } else {
      verifiers_[p.first] = iter->second;
    }
    if (replicasInfo_.isIdOfExternalClient(p.first)) {
      clientsPublicKeys_.ids_to_keys[p.first] = concord::messages::keys_and_signatures::PublicKey{key, (uint8_t)format};
      LOG_DEBUG(KEY_EX_LOG, "Adding key of client " << p.first << " key size " << key.size());
    }
  }
  clientsPublicKeys_.version = 2;  // version `1` suggests RSAVerifier.
  LOG_DEBUG(KEY_EX_LOG, "Map contains " << clientsPublicKeys_.ids_to_keys.size() << " public clients keys");
  metrics_component_.Register();

  // This is done mainly for debugging and sanity check:
  // compute a vector which counts how many participants and which are per each key:
  vector<set<PrincipalId>> keyIndexToPrincipalIds(publickeys.size());
  for (auto& principalIdToKeyIndex : publicKeysMapping) {
    ConcordAssert(principalIdToKeyIndex.second < keyIndexToPrincipalIds.size());
    keyIndexToPrincipalIds[principalIdToKeyIndex.second].insert(principalIdToKeyIndex.first);
  }
  size_t i{0};
  for (auto& ids : keyIndexToPrincipalIds) {
    // Knowing how deplyment works, we assume a continuous ids per key. If not, the next log line is not sufficient
    LOG_INFO(GL,
             "Key index " << i << " is used by " << ids.size() << " principal IDs"
                          << " from " << (*std::min_element(ids.begin(), ids.end())) << " to "
                          << (*std::max_element(ids.begin(), ids.end())));
    ++i;
  }

  LOG_INFO(GL,
           "SigManager initialized: " << KVLOG(myId_, verifiers_.size(), publicKeyIndexToVerifier.size()));
  ConcordAssert(verifiers_.size() >= publickeys.size());
}

uint16_t SigManager::getSigLength(PrincipalId pid) const {
  if (pid == myId_) {
    return (uint16_t)mySigner_->signatureLength();
  } else {
    std::shared_lock lock(mutex_);
    if (auto pos = verifiers_.find(pid); pos != verifiers_.end()) {
      auto result = pos->second->signatureLength();
      // LOG_INFO(GL, "Sig size for id: " << pid << "is " << result);
      return result;
    } else {
      LOG_ERROR(GL, "Unrecognized pid " << pid);
      return 0;
    }
  }
}

bool SigManager::verifyNonReplicaSig(
    PrincipalId pid, const concord::Byte* data, size_t dataLength, const concord::Byte* sig, uint16_t sigLength) const {
  bool result = false;
  {
    std::shared_lock lock(mutex_);
    ConcordAssert(!replicasInfo_.isIdOfReplica(myId_) || !replicasInfo_.isIdOfReplica(pid));

    if (auto pos = verifiers_.find(pid); pos != verifiers_.end()) {
      result = pos->second->verifyBuffer(data, dataLength, sig, sigLength);
    } else {
      LOG_ERROR(GL, "Unrecognized pid " << pid);
      metrics_.sigVerificationFailedOnUnrecognizedParticipantId_++;
      metrics_component_.UpdateAggregator();
      return false;
    }
  }
  bool idOfReplica = false, idOfExternalClient = false, idOfReadOnlyReplica = false;
  idOfExternalClient = replicasInfo_.isIdOfExternalClient(pid);
  if (!idOfExternalClient) {
    idOfReplica = replicasInfo_.isIdOfReplica(pid);
  }
  idOfReadOnlyReplica = replicasInfo_.isIdOfPeerRoReplica(pid);
  ConcordAssert(idOfReplica || idOfExternalClient || idOfReadOnlyReplica);
  if (!result) {  // failure
    if (idOfExternalClient)
      metrics_.externalClientReqSigVerificationFailed_++;
    else
      metrics_.replicaSigVerificationFailed_++;
    metrics_component_.UpdateAggregator();
  } else {  // success
    if (idOfExternalClient) {
      metrics_.externalClientReqSigVerified_++;
      if ((metrics_.externalClientReqSigVerified_.Get().Get() % updateMetricsAggregatorThresh) == 0)
        metrics_component_.UpdateAggregator();
    } else {
      metrics_.replicaSigVerified_++;
      if ((metrics_.replicaSigVerified_.Get().Get() % updateMetricsAggregatorThresh) == 0)
        metrics_component_.UpdateAggregator();
    }
  }
  return result;
}


size_t SigManager::sign(SeqNum seq, const concord::Byte* data, size_t dataLength, concord::Byte* outSig) const {
  ConcordAssert(replicasInfo_.isIdOfReplica(myId_));
  auto& signer = *reinterpret_cast<EdDSAMultisigSigner*>(CryptoManager::instance().getSigner(seq));
  auto result = signer.signBuffer(data, dataLength, outSig);
  LOG_INFO(GL, "Signing as replica with " << KVLOG(myId_, seq, signer.signatureLength(), result));
  return result;
}

size_t SigManager::sign(SeqNum seq, const char* data, size_t dataLength, char* outSig) const {
  return sign(seq, reinterpret_cast<const uint8_t*>(data), dataLength, reinterpret_cast<uint8_t*>(outSig));
}

bool SigManager::verifyReplicaSig(PrincipalId replicaID,
                      const concord::Byte* data,
                      size_t dataLength,
                      const concord::Byte* sig,
                      uint16_t sigLength) const {
  ConcordAssert(replicasInfo_.isIdOfReplica(myId_));
  ConcordAssert(replicasInfo_.isIdOfReplica(replicaID));
  for (auto multisigVerifier : CryptoManager::instance().getLatestVerifiers()) {
    auto& verifier = reinterpret_cast<EdDSAMultisigVerifier*>(multisigVerifier.get())->getVerifier(replicaID);
    LOG_INFO(GL, "Validating as replica with: " << KVLOG(myId_, replicaID, sigLength, verifier.signatureLength()));
    printCallStack();
    if (verifier.verifyBuffer(data, dataLength, sig, sigLength)) {
      LOG_INFO(GL, "Validation Successful " << KVLOG(myId_, replicaID, sigLength, verifier.signatureLength()));
      return true;
    } else {
      LOG_INFO(GL, "Validation failed as replica with: " << KVLOG(replicaID, sigLength, verifier.signatureLength()));
    }
  }
  return false;

}

// verify using the two last keys, once the last key's checkpoint is reached, the previous key is removed
bool SigManager::verifySig(
    PrincipalId pid, const concord::Byte* data, size_t dataLength, const concord::Byte* sig, uint16_t sigLength) const {
  if (replicasInfo_.isIdOfReplica(pid)) {
    return verifyReplicaSig(pid, data, dataLength, sig, sigLength);
  }

  return verifyNonReplicaSig(pid, data, dataLength, sig, sigLength);
}

bool SigManager::verifyOwnSignature(const concord::Byte* data,
                                           size_t dataLength,
                                           const concord::Byte* expectedSignature) const {
  std::vector<concord::Byte> sig(getMySigLength());
  for (auto multisigSigner : CryptoManager::instance().getLatestSigners()) {
    auto* signer = reinterpret_cast<EdDSAMultisigSigner*>(multisigSigner.get());
    signer->signBuffer(data, dataLength, sig.data());

    if (std::memcmp(sig.data(), expectedSignature, getMySigLength()) == 0) {
      LOG_INFO(GL, "Self-sig validation succeeded");
      return true;
    } else {
      LOG_INFO(GL, "Self-sig validation failed");
    }
  }

  return false;
}

uint16_t SigManager::getMySigLength() const {
  if (replicasInfo_.isIdOfReplica(myId_)) {
    return getLatestReplicaSigner()->signatureLength();
  }
  return (uint16_t)mySigner_->signatureLength();
}

void SigManager::setClientPublicKey(const std::string& key, PrincipalId id, KeyFormat format) {
  LOG_INFO(KEY_EX_LOG, "client: " << id << " key: " << key << " format: " << (uint16_t)format);
  if (replicasInfo_.isIdOfExternalClient(id) || replicasInfo_.isIdOfClientService(id)) {
    try {
      std::unique_lock lock(mutex_);
      verifiers_.insert_or_assign(id,
                                  std::shared_ptr<IVerifier>(Factory::getVerifier(
                                      key, ReplicaConfig::instance().replicaMsgSigningAlgo, format)));
    } catch (const std::exception& e) {
      LOG_ERROR(KEY_EX_LOG, "failed to add a key for client: " << id << " reason: " << e.what());
      throw;
    }
    clientsPublicKeys_.ids_to_keys[id] = concord::messages::keys_and_signatures::PublicKey{key, (uint8_t)format};
  } else {
    LOG_WARN(KEY_EX_LOG, "Illegal id for client " << id);
  }
}
bool SigManager::hasVerifier(PrincipalId pid) { return verifiers_.find(pid) != verifiers_.end(); }

concord::crypto::SignatureAlgorithm SigManager::getMainKeyAlgorithm() const { return concord::crypto::EdDSA; }
concord::crypto::ISigner* SigManager::getLatestReplicaSigner() const {
  auto latestSigners = CryptoManager::instance().getLatestSigners();
  auto ret = static_cast<EdDSAMultisigSigner*>(latestSigners.begin()->get());
  LOG_INFO(GL, KVLOG((uint64_t)ret));
  return ret;
}

std::string SigManager::getSelfPrivKey() const {
  return getLatestReplicaSigner()->getPrivKey();
}

const concord::crypto::IVerifier& SigManager::getVerifier(PrincipalId otherPrincipal) const {
  return *verifiers_.at(otherPrincipal);
}

void SigManager::setReplicaLastExecutedSeq(SeqNum seq) {
  ConcordAssert(replicasInfo_.isIdOfReplica(myId_));
  replicaLastExecutedSeq_ = seq;
}

SeqNum SigManager::getReplicaLastExecutedSeq() {
  ConcordAssert(replicasInfo_.isIdOfReplica(myId_));
  return replicaLastExecutedSeq_;
}

}  // namespace impl
}  // namespace bftEngine

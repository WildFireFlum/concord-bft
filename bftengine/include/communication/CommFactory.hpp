// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#ifndef BFTENGINE_INCLUDE_COMMUNICATION_COMMFACTORY_HPP_
#define BFTENGINE_INCLUDE_COMMUNICATION_COMMFACTORY_HPP_

#include "CommDefs.hpp"
#include "Logging.hpp"

namespace bftEngine {
class CommFactory {  
 private:
  static concordlogger::Logger _logger;
 public:
  static ICommunication*
  create(const BaseCommConfig &config);
};
}  // namespace bftEngine

#endif  // BFTENGINE_INCLUDE_COMMUNICATION_COMMFACTORY_HPP_

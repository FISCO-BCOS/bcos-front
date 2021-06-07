/*
 *  Copyright (C) 2021 FISCO BCOS.
 *  SPDX-License-Identifier: Apache-2.0
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * @file FrontServiceFactory.cpp
 * @author: octopus
 * @date 2021-05-20
 */

#include <bcos-framework/libutilities/Exceptions.h>
#include <bcos-front/Common.h>
#include <bcos-front/FrontService.h>
#include <bcos-front/FrontServiceFactory.h>

using namespace bcos;
using namespace front;

FrontService::Ptr FrontServiceFactory::buildFrontService() {
  if (m_groupID.empty()) {
    BOOST_THROW_EXCEPTION(
        InvalidParameter() << errinfo_comment(
            "FrontServiceFactory::init groupID is uninitialized"));
  }

  if (!m_nodeID) {
    BOOST_THROW_EXCEPTION(
        InvalidParameter() << errinfo_comment(
            "FrontServiceFactory::init nodeID is uninitialized"));
  }

  if (!m_gatewayInterface) {
    BOOST_THROW_EXCEPTION(
        InvalidParameter() << errinfo_comment(
            "FrontServiceFactory::init gateway is uninitialized"));
  }

  if (!m_threadPool) {
    BOOST_THROW_EXCEPTION(
        InvalidParameter() << errinfo_comment(
            "FrontServiceFactory::init threadPool is uninitialized"));
  }

  FRONT_LOG(INFO) << LOG_DESC("FrontServiceFactory::init")
                  << LOG_KV("groupID", m_groupID)
                  << LOG_KV("nodeID", m_nodeID->hex());

  auto factory = std::make_shared<FrontMessageFactory>();
  auto ioService = std::make_shared<boost::asio::io_service>();
  auto frontService = std::make_shared<FrontService>();

  frontService->setMessageFactory(factory);
  frontService->setGroupID(m_groupID);
  frontService->setNodeID(m_nodeID);
  frontService->setIoService(ioService);
  frontService->setGatewayInterface(m_gatewayInterface);
  frontService->setThreadPool(m_threadPool);

  return frontService;
}
/**
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
 * @brief test for front service
 * @file FrontServiceTest.h
 * @author: octopus
 * @date 2021-04-26
 */

#include "front/FrontService.h"
#include "FakeGateway.h"
#include "FakeKeyInterface.h"
#include "front/Common.h"
#include "front/FrontMessage.h"
#include <bcos-framework/interfaces/protocol/CommonError.h>
#include <bcos-test/libutils/TestPromptFixture.h>
#include <boost/test/unit_test.hpp>

using namespace bcos;
using namespace bcos::test;
using namespace bcos::front;
using namespace bcos::front::test;

const static std::string g_groupID = "front.service.group";
const static std::string g_srcNodeID = "front.src.nodeid";
const static std::string g_dstNodeID_0 = "front.dst.nodeid.0";
const static std::string g_dstNodeID_1 = "front.dst.nodeid.1";

std::shared_ptr<FrontService> createFrontService() {

  auto gateway = std::make_shared<FakeGateway>();
  auto factory = std::make_shared<FrontMessageFactory>();
  auto srcNodeID = std::make_shared<FakeKeyInterface>(g_srcNodeID);
  auto ioService = std::make_shared<boost::asio::io_service>();
  auto threadPool = std::make_shared<ThreadPool>("frontServiceTest", 16);

  auto frontService = std::make_shared<FrontService>();
  frontService->setGroupID(g_groupID);
  frontService->setMessageFactory(factory);
  frontService->setGatewayInterface(gateway);
  frontService->setIoService(ioService);
  frontService->setNodeID(srcNodeID);
  frontService->setThreadPool(threadPool);
  frontService->start();

  return frontService;
}

std::shared_ptr<FrontService> createEmptyFrontService() {
  auto frontService = std::make_shared<FrontService>();
  return frontService;
}

BOOST_FIXTURE_TEST_SUITE(FrontServiceTest, TestPromptFixture)

// test
BOOST_AUTO_TEST_CASE(testFrontService_createEmptyFrontService) {
  auto frontService = createEmptyFrontService();
  BOOST_CHECK_EQUAL(frontService->groupID(), "");
  BOOST_CHECK(!frontService->nodeID());
  BOOST_CHECK(!frontService->gatewayInterface());
  BOOST_CHECK(!frontService->messageFactory());
  BOOST_CHECK(!frontService->ioService());
  BOOST_CHECK(frontService->mapMessageDispatcher().empty());
  BOOST_CHECK(frontService->mapNodeStatusNotifier().empty());
  BOOST_CHECK(frontService->callback().empty());
}

BOOST_AUTO_TEST_CASE(testFrontService_createFrontService) {
  auto frontService = createFrontService();
  BOOST_CHECK_EQUAL(frontService->groupID(), g_groupID);
  BOOST_CHECK_EQUAL(frontService->nodeID()->hex(), g_srcNodeID);
  BOOST_CHECK(frontService->gatewayInterface());
  BOOST_CHECK(frontService->messageFactory());
  BOOST_CHECK(frontService->ioService());
  BOOST_CHECK(frontService->mapMessageDispatcher().empty());
  BOOST_CHECK(frontService->mapNodeStatusNotifier().empty());
  BOOST_CHECK(frontService->callback().empty());
}

BOOST_AUTO_TEST_CASE(testFrontService_registerMessageDispater) {
  auto frontService = createFrontService();
  int moduleID = 111;

  auto callback = [&](Error::Ptr _error, bcos::crypto::NodeIDPtr _nodeID,
                      bytesConstRef _data,
                      std::function<void(bytesConstRef _respData)> _respFunc) {
    (void)_error;
    (void)_nodeID;
    (void)_data;
    (void)_respFunc;
  };

  frontService->registerMessageDispatcher(moduleID, callback);
  frontService->registerMessageDispatcher(moduleID + 1, callback);
  frontService->registerMessageDispatcher(moduleID + 2, callback);
  BOOST_CHECK(frontService->mapMessageDispatcher().size() == 3);
  BOOST_CHECK(frontService->mapMessageDispatcher().find(moduleID) !=
              frontService->mapMessageDispatcher().end());
  BOOST_CHECK(frontService->mapMessageDispatcher().find(moduleID + 4) ==
              frontService->mapMessageDispatcher().end());
}

BOOST_AUTO_TEST_CASE(testFrontService_registerNodeStatusNotifier) {
  auto frontService = createFrontService();
  int moduleID = 113;

  auto callback = [&](Error::Ptr _error) { (void)_error; };

  frontService->registerNodeStatusNotifier(moduleID, callback);
  frontService->registerNodeStatusNotifier(moduleID + 1, callback);
  frontService->registerNodeStatusNotifier(moduleID + 2, callback);
  BOOST_CHECK(frontService->mapNodeStatusNotifier().size() == 3);
  BOOST_CHECK(frontService->mapNodeStatusNotifier().find(moduleID) !=
              frontService->mapNodeStatusNotifier().end());
  BOOST_CHECK(frontService->mapNodeStatusNotifier().find(moduleID + 4) ==
              frontService->mapNodeStatusNotifier().end());
}

BOOST_AUTO_TEST_CASE(testFrontService_asyncSendMessageByNodeID_no_callback) {
  auto frontService = createFrontService();
  auto gateway =
      std::static_pointer_cast<FakeGateway>(frontService->gatewayInterface());

  int moduleID = 111;
  auto dstNodeID = std::make_shared<FakeKeyInterface>(g_dstNodeID_0);
  std::string data(1000, 'x');

  frontService->asyncSendMessageByNodeID(
      moduleID, dstNodeID,
      bytesConstRef((unsigned char *)data.data(), data.size()), 0,
      CallbackFunc());

  BOOST_CHECK(frontService->callback().empty());

  auto groupID = gateway->groupID();
  auto nodeID = gateway->nodeID();
  auto payload = gateway->payload();

  BOOST_CHECK_EQUAL(groupID, g_groupID);
  BOOST_CHECK_EQUAL(dstNodeID->hex(), nodeID->hex());

  auto message = frontService->messageFactory()->buildMessage();
  auto ret = message->decode(bytesConstRef(payload->data(), payload->size()));

  BOOST_CHECK_EQUAL(message->payload().size(), data.size());
  auto uuid = std::string(message->uuid()->begin(), message->uuid()->end());

  BOOST_CHECK_EQUAL(ret, MessageDecodeStatus::MESSAGE_COMPLETE);
  BOOST_CHECK(uuid.empty());
  BOOST_CHECK_EQUAL(message->moduleID(), moduleID);
  BOOST_CHECK_EQUAL(
      std::string(message->payload().begin(), message->payload().end()), data);
}

BOOST_AUTO_TEST_CASE(testFrontService_asyncSendMessageByNodeID_callback) {
  auto frontService = createFrontService();
  auto gateway =
      std::static_pointer_cast<FakeGateway>(frontService->gatewayInterface());
  auto message = frontService->messageFactory()->buildMessage();

  int moduleID = 222;
  auto dstNodeID = std::make_shared<FakeKeyInterface>(g_dstNodeID_0);
  std::string data(1000, '#');

  auto okPtr =
      std::make_shared<Error>(bcos::protocol::CommonError::SUCCESS, "success");

  BOOST_CHECK(frontService->callback().empty());

  {
    std::promise<void> barrier;
    auto callback =
        [&](Error::Ptr _error, bcos::crypto::NodeIDPtr _nodeID,
            bytesConstRef _data,
            std::function<void(bytesConstRef _respData)> _respFunc) {
          (void)_error;
          (void)_nodeID;
          (void)_data;
          (void)_respFunc;
          BOOST_CHECK_EQUAL(_error->errorCode(), okPtr->errorCode());
          barrier.set_value();
        };

    frontService->asyncSendMessageByNodeID(
        moduleID, dstNodeID,
        bytesConstRef((unsigned char *)data.data(), data.size()), 1000,
        callback);

    BOOST_CHECK(frontService->callback().size() == 1);

    auto groupID = gateway->groupID();
    auto nodeID = gateway->nodeID();
    auto payload = gateway->payload();

    BOOST_CHECK_EQUAL(groupID, g_groupID);
    BOOST_CHECK_EQUAL(dstNodeID->hex(), nodeID->hex());

    auto ret = message->decode(bytesConstRef(payload->data(), payload->size()));
    auto uuid = std::string(message->uuid()->begin(), message->uuid()->end());

    BOOST_CHECK_EQUAL(ret, MessageDecodeStatus::MESSAGE_COMPLETE);
    BOOST_CHECK_EQUAL(message->moduleID(), moduleID);
    BOOST_CHECK_EQUAL(
        std::string(message->payload().begin(), message->payload().end()),
        data);
    BOOST_CHECK(frontService->callback().find(uuid) !=
                frontService->callback().end());

    // receive message
    frontService->onReceiveMessage(
        okPtr, nodeID, bytesConstRef(payload->data(), payload->size()));

    std::future<void> barrier_future = barrier.get_future();
    barrier_future.wait();

    BOOST_CHECK(frontService->callback().find(uuid) ==
                frontService->callback().end());
  }
}

BOOST_AUTO_TEST_CASE(testFrontService_asyncSendMessageByNodeIDcmak_timeout) {
  auto frontService = createFrontService();
  auto gateway =
      std::static_pointer_cast<FakeGateway>(frontService->gatewayInterface());
  auto message = frontService->messageFactory()->buildMessage();

  int moduleID = 222;
  auto dstNodeID = std::make_shared<FakeKeyInterface>(g_dstNodeID_0);
  std::string data(1000, '#');

  BOOST_CHECK(frontService->callback().empty());

  {
    std::promise<void> barrier;
    Error::Ptr _error;
    auto callback =
        [&](Error::Ptr _error, bcos::crypto::NodeIDPtr _nodeID,
            bytesConstRef _data,
            std::function<void(bytesConstRef _respData)> _respFunc) {
          (void)_nodeID;
          (void)_data;
          (void)_respFunc;
          BOOST_CHECK_EQUAL(_error->errorCode(),
                            bcos::protocol::CommonError::TIMEOUT);
          barrier.set_value();
        };

    frontService->asyncSendMessageByNodeID(
        moduleID, dstNodeID,
        bytesConstRef((unsigned char *)data.data(), data.size()), 1000,
        callback);

    BOOST_CHECK(frontService->callback().size() == 1);

    auto groupID = gateway->groupID();
    auto nodeID = gateway->nodeID();
    auto payload = gateway->payload();

    BOOST_CHECK_EQUAL(groupID, g_groupID);
    BOOST_CHECK_EQUAL(dstNodeID->hex(), nodeID->hex());

    auto ret = message->decode(bytesConstRef(payload->data(), payload->size()));
    auto uuid = std::string(message->uuid()->begin(), message->uuid()->end());

    BOOST_CHECK_EQUAL(ret, MessageDecodeStatus::MESSAGE_COMPLETE);
    BOOST_CHECK_EQUAL(message->moduleID(), moduleID);
    BOOST_CHECK_EQUAL(
        std::string(message->payload().begin(), message->payload().end()),
        data);
    BOOST_CHECK(frontService->callback().find(uuid) !=
                frontService->callback().end());

    std::future<void> barrier_future = barrier.get_future();
    barrier_future.wait();

    BOOST_CHECK(frontService->callback().find(uuid) ==
                frontService->callback().end());
  }
}

BOOST_AUTO_TEST_CASE(testFrontService_asyncMulticastMessage) {
  auto frontService = createFrontService();
  auto gateway =
      std::static_pointer_cast<FakeGateway>(frontService->gatewayInterface());

  int moduleID = 222;
  auto dstNodeID = std::make_shared<FakeKeyInterface>(g_dstNodeID_0);
  std::string data(1000, 'z');

  frontService->asyncMulticastMessage(
      moduleID, bytesConstRef((unsigned char *)data.data(), data.size()));

  auto groupID = gateway->groupID();
  auto payload = gateway->payload();

  BOOST_CHECK_EQUAL(groupID, g_groupID);

  auto message = frontService->messageFactory()->buildMessage();
  auto ret = message->decode(bytesConstRef(payload->data(), payload->size()));

  BOOST_CHECK_EQUAL(message->payload().size(), data.size());

  BOOST_CHECK_EQUAL(ret, MessageDecodeStatus::MESSAGE_COMPLETE);
  BOOST_CHECK_EQUAL(message->moduleID(), moduleID);
  BOOST_CHECK_EQUAL(
      std::string(message->payload().begin(), message->payload().end()), data);
}

BOOST_AUTO_TEST_CASE(testFrontService_asyncSendMessageByNodeIDs) {
  auto frontService = createFrontService();
  auto gateway =
      std::static_pointer_cast<FakeGateway>(frontService->gatewayInterface());

  int moduleID = 333;
  auto dstNodeID = std::make_shared<FakeKeyInterface>(g_dstNodeID_0);
  std::string data(1000, 'y');

  frontService->asyncSendMessageByNodeIDs(
      moduleID, bcos::crypto::NodeIDs{dstNodeID},
      bytesConstRef((unsigned char *)data.data(), data.size()));

  auto groupID = gateway->groupID();
  auto payload = gateway->payload();

  BOOST_CHECK_EQUAL(groupID, g_groupID);

  auto message = frontService->messageFactory()->buildMessage();
  auto ret = message->decode(bytesConstRef(payload->data(), payload->size()));

  BOOST_CHECK_EQUAL(message->payload().size(), data.size());

  BOOST_CHECK_EQUAL(ret, MessageDecodeStatus::MESSAGE_COMPLETE);
  BOOST_CHECK_EQUAL(message->moduleID(), moduleID);
  BOOST_CHECK_EQUAL(
      std::string(message->payload().begin(), message->payload().end()), data);
}

BOOST_AUTO_TEST_CASE(testFrontService_registerMessageDispater_callback) {
  auto frontService = createFrontService();
  auto gateway =
      std::static_pointer_cast<FakeGateway>(frontService->gatewayInterface());

  auto message = frontService->messageFactory()->buildMessage();
  auto srcNodeID = std::make_shared<FakeKeyInterface>(g_srcNodeID);

  std::string data(1000, '#');

  auto okPtr =
      std::make_shared<Error>(bcos::protocol::CommonError::SUCCESS, "success");

  bcos::crypto::NodeIDPtr nodeID = nullptr;
  auto payloadPtr = std::make_shared<bytes>();

  std::promise<void> barrier;
  auto callback =
      [&nodeID, &payloadPtr, &barrier,
       okPtr](Error::Ptr _error, bcos::crypto::NodeIDPtr _nodeID,
              bytesConstRef _data,
              std::function<void(bytesConstRef _respData)> _respFunc) mutable {
        nodeID = _nodeID;
        payloadPtr->insert(payloadPtr->end(), _data.begin(), _data.end());
        (void)_respFunc;
        BOOST_CHECK_EQUAL(_error->errorCode(), okPtr->errorCode());
        barrier.set_value();
      };

  int moduleID = 222;
  int ext = 333;
  frontService->registerMessageDispatcher(moduleID, callback);

  message->setModuleID(moduleID);
  message->setExt(ext);
  message->setPayload(bytesConstRef((byte *)data.data(), data.size()));
  auto buffer = std::make_shared<bytes>();
  message->encode(*buffer.get());

  // receive message
  frontService->onReceiveMessage(okPtr, srcNodeID,
                                 bytesConstRef(buffer->data(), buffer->size()));

  std::future<void> barrier_future = barrier.get_future();
  barrier_future.wait();

  BOOST_CHECK(!payloadPtr->empty());
  BOOST_CHECK_EQUAL(std::string(payloadPtr->begin(), payloadPtr->end()), data);
}

BOOST_AUTO_TEST_CASE(testFrontService_multi_timeout) {
  auto frontService = createFrontService();
  auto gateway =
      std::static_pointer_cast<FakeGateway>(frontService->gatewayInterface());
  auto message = frontService->messageFactory()->buildMessage();

  int moduleID = 222;
  auto dstNodeID = std::make_shared<FakeKeyInterface>(g_dstNodeID_0);
  std::string data(1000, '#');

  BOOST_CHECK(frontService->callback().empty());

  std::vector<std::promise<void>> barriers;
  barriers.resize(1000);

  for (auto &barrier : barriers) {
    Error::Ptr _error;
    auto callback =
        [&](Error::Ptr _error, bcos::crypto::NodeIDPtr _nodeID,
            bytesConstRef _data,
            std::function<void(bytesConstRef _respData)> _respFunc) {
          (void)_nodeID;
          (void)_data;
          (void)_respFunc;
          BOOST_CHECK_EQUAL(_error->errorCode(),
                            bcos::protocol::CommonError::TIMEOUT);
          barrier.set_value();
        };

    frontService->asyncSendMessageByNodeID(
        moduleID, dstNodeID,
        bytesConstRef((unsigned char *)data.data(), data.size()), 2000,
        callback);
  }

  BOOST_CHECK(frontService->callback().size() == barriers.size());

  for (auto &barrier : barriers) {
    std::future<void> barrier_future = barrier.get_future();
    barrier_future.wait();
  }

  BOOST_CHECK(frontService->callback().empty());
}

BOOST_AUTO_TEST_SUITE_END()

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
 * @file FrontService.cpp
 * @author: octopus
 * @date 2021-04-19
 */

#include "FrontService.h"
#include "Common.h"
#include "FrontMessage.h"
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace bcos;
using namespace front;

FrontService::FrontService() {
  FRONT_LOG(INFO) << LOG_DESC("[FrontService::FrontService] ")
                  << LOG_KV("this", this);
}

FrontService::~FrontService() {
  FRONT_LOG(INFO) << LOG_DESC("[FrontService::~FrontService] ")
                  << LOG_KV("this", this);
}

void FrontService::start() {
  if (m_run) {
    FRONT_LOG(INFO) << LOG_DESC("[FrontService::start] FrontService is running")
                    << LOG_KV("nodeID", stringNodeID())
                    << LOG_KV("groupID", m_groupID);
    return;
  }

  // TODO: check if FrontService is initialized correctly
  // groupID
  // nodeID

  FRONT_LOG(INFO) << LOG_DESC("[FrontService::start] ")
                  << LOG_KV("nodeID", stringNodeID())
                  << LOG_KV("groupID", m_groupID);
  m_run = true;
  return;
}
void FrontService::stop() {

  if (!m_run) {
    return;
  }

  FRONT_LOG(INFO) << LOG_DESC("[FrontService::stop] ")
                  << LOG_KV("nodeID", stringNodeID())
                  << LOG_KV("groupID", m_groupID);

  if (m_threadPool) {
    m_threadPool->stop();
  }

  m_run = false;
  return;
}

/**
 * @brief: get nodeID list
 * @return void
 */
void FrontService::asyncGetNodeIDs(
    std::function<void(Error::Ptr _error,
                       const std::shared_ptr<const crypto::NodeIDs> &)>
        _getNodeIDsFunc) const {
  // TODO: fetch from gateway or the front service saves itself
}

/**
 * @brief: send message to node
 * @param _moduleID: moduleID
 * @param _nodeID: the receiver nodeID
 * @param _data: send data
 * @param _timeout: the timeout value of async function, in milliseconds.
 * @param _callback: callback
 * @return void
 */
void FrontService::asyncSendMessageByNodeID(int _moduleID,
                                            bcos::crypto::NodeIDPtr _nodeID,
                                            bytesConstRef _data,
                                            uint32_t _timeout,
                                            CallbackFunc _callback) {

  std::string uuid;

  if (_callback) {
    uuid = boost::uuids::to_string(boost::uuids::random_generator()());

    auto callback = std::make_shared<Callback>();
    callback->callbackFunc = _callback;

    if (_timeout > 0) {
      // create new timer to handle timeout
      auto timeoutHandler = std::make_shared<boost::asio::deadline_timer>(
          *m_ioService, boost::posix_time::milliseconds(_timeout));

      callback->timeoutHandler = timeoutHandler;
      // callback->startTime = utcSteadyTime();

      timeoutHandler->async_wait(
          boost::bind(&FrontService::onMessageTimeout, shared_from_this(),
                      boost::asio::placeholders::error, uuid));
    }

    addCallback(uuid, callback);

    FRONT_LOG(DEBUG) << ("FrontService::asyncSendMessageByNodeID")
                     << LOG_KV("uuid", uuid) << LOG_KV("nodeID", m_stringNodeID)
                     << LOG_KV("timeout", _timeout);

  } // if (_callback)

  onSendMessage(_moduleID, _nodeID, uuid, _data);
}

/**
 * @brief: send message to multiple nodes
 * @param _moduleID: moduleID
 * @param _nodeIDs: the receiver nodeIDs
 * @param _data: send data
 * @return void
 */
void FrontService::asyncSendMessageByNodeIDs(int _moduleID,
                                             const crypto::NodeIDs &_nodeIDs,
                                             bytesConstRef _data) {
  for (const auto &_nodeID : _nodeIDs) {
    asyncSendMessageByNodeID(_moduleID, _nodeID, _data, 0, CallbackFunc());
  }
}

/**
 * @brief: send broadcast message
 * @param _moduleID: moduleID
 * @param _data:  send data
 * @return void
 */
void FrontService::asyncMulticastMessage(int _moduleID, bytesConstRef _data) {

  auto message = messageFactory()->buildMessage();
  message->setModuleID(_moduleID);
  message->setPayload(_data);

  auto buffer = std::make_shared<bytes>();
  message->encode(*buffer.get());

  m_gatewayInterface->asyncMulticastMessage(
      m_groupID, bytesConstRef(buffer->data(), buffer->size()));
}

/**
 * @brief: register the node change callback
 * @param _moduleID: moduleID
 * @param _notifier: callback
 * @return void
 */
void FrontService::registerNodeStatusNotifier(int _moduleID,
                                              NodeStatusNotifier _notifier) {
  m_mapNodeStatusNotifier[_moduleID] = _notifier;

  FRONT_LOG(INFO) << LOG_DESC("[FrontService::registerNodeStatusNotifier] ")
                  << LOG_KV("moduleID", _moduleID)
                  << LOG_KV("nodeID", m_stringNodeID)
                  << LOG_KV("groupID", m_groupID);
}

/**
 * @brief: register the callback for module message
 * @param _moduleID: moduleID
 * @param _dispatcher: callback
 * @return void
 */
void FrontService::registerMessageDispatcher(int _moduleID,
                                             MessageDispatcher _dispatcher) {
  m_mapMessageDispatcher[_moduleID] = _dispatcher;

  FRONT_LOG(INFO) << LOG_DESC("[FrontService::registerMessageDispatcher] ")
                  << LOG_KV("moduleID", _moduleID)
                  << LOG_KV("nodeID", m_stringNodeID)
                  << LOG_KV("groupID", m_groupID);
}

/**
 * @brief: receive message from gateway
 * @param _data: callback
 * @return void
 */
void FrontService::onReceiveMessage(bcos::crypto::NodeIDPtr _nodeID,
                                    bytesConstRef _data) {

  auto message = messageFactory()->buildMessage();
  auto result = message->decode(_data);
  if (MessageDecodeStatus::MESSAGE_COMPLETE != result) {
    FRONT_LOG(ERROR)
        << LOG_DESC("[FrontService::onReceiveMessage] invalid message format")
        << LOG_KV("messageLength", _data.size())
        << LOG_KV("fromNodeID", stringNodeID());
    return;
  }

  int moduleID = message->moduleID();
  int ext = message->ext();
  std::shared_ptr<bytes> payload = std::make_shared<bytes>(
      message->payload().begin(), message->payload().end());
  std::string uuid =
      std::string(message->uuid()->begin(), message->uuid()->end());

  FRONT_LOG(DEBUG) << LOG_DESC("[FrontService::onReceiveMessage] pacaket info ")
                   << LOG_KV("moduleID", moduleID) << LOG_KV("uuid", uuid)
                   << LOG_KV("ext", ext) << LOG_KV("groupID", m_groupID)
                   << LOG_KV("fromNodeID", stringNodeID());

  // response func
  auto _respFunc = std::bind(&FrontService::onSendMessage, shared_from_this(),
                             moduleID, _nodeID, uuid, std::placeholders::_1);

  if (!uuid.empty()) {
    // check if callback message
    auto callback = getAndRemoveCallback(uuid);
    if (callback) {
      // cancel the timer first
      callback->timeoutHandler->cancel();
      if (m_threadPool) {
        m_threadPool->enqueue([=] {
          callback->callbackFunc(
              nullptr, _nodeID, bytesConstRef(payload->data(), payload->size()),
              _respFunc);
        });
      } else {
        callback->callbackFunc(nullptr, _nodeID,
                               bytesConstRef(payload->data(), payload->size()),
                               _respFunc);
      }
      return;
    }
  }

  // message dispatch to each module based on the callback registered
  // by each module
  auto moduleCallback = m_mapMessageDispatcher.find(moduleID);
  if (moduleCallback != m_mapMessageDispatcher.end()) {
    if (m_threadPool) {
      m_threadPool->enqueue([=] {
        moduleCallback->second(nullptr, _nodeID,
                               bytesConstRef(payload->data(), payload->size()),
                               _respFunc);
      });
    } else {
      moduleCallback->second(nullptr, _nodeID,
                             bytesConstRef(payload->data(), payload->size()),
                             _respFunc);
    }
  } else {
    FRONT_LOG(WARNING)
        << LOG_DESC("[onReceiveMessage] found no registered module callback")
        << LOG_KV("moduleID", moduleID);
  }
}

/**
 * @brief: send message
 * @param _moduleID: moduleID
 * @param _nodeID: the node where the message needs to be sent to
 * @param _data: uuid
 * @param _data: message content
 * @return void
 */
void FrontService::onSendMessage(int _moduleID, bcos::crypto::NodeIDPtr _nodeID,
                                 const std::string &_uuid,
                                 bytesConstRef _data) {

  auto message = messageFactory()->buildMessage();
  message->setModuleID(_moduleID);
  message->setUuid(std::make_shared<bytes>(_uuid.begin(), _uuid.end()));
  message->setPayload(_data);

  auto buffer = std::make_shared<bytes>();
  message->encode(*buffer.get());

  // call gateway interface to send the message
  m_gatewayInterface->asyncSendMessageByNodeID(
      m_groupID, _nodeID, bytesConstRef(buffer->data(), buffer->size()), 0,
      gateway::CallbackFunc());
}

/**
 * @brief: handle message timeout
 * @param _error: boost error code
 * @param _uuid: message uuid
 * @return void
 */
void FrontService::onMessageTimeout(const boost::system::error_code &_error,
                                    const std::string &_uuid) {
  if (_error) {
    FRONT_LOG(ERROR)
        << LOG_DESC("[FrontService::onMessageTimeout] async_await error")
        << LOG_KV("error", _error) << LOG_KV("uuid", _uuid);
    return;
  }

  Callback::Ptr callback = getAndRemoveCallback(_uuid);
  if (callback) {
    // TODO: timeout error define
    // callback->callbackFunc(nullptr, );
  }

  FRONT_LOG(WARNING) << LOG_DESC(
                            "[FrontService::onMessageTimeout] message timeout")
                     << LOG_KV("uuid", _uuid);
}
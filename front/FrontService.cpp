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

#include <bcos-framework/interfaces/protocol/CommonError.h>
#include <bcos-framework/libutilities/Common.h>
#include <bcos-framework/libutilities/Exceptions.h>

using namespace bcos;
using namespace front;
using namespace protocol;

FrontService::FrontService() {
  // m_ioService = std::make_shared<boost::asio::io_service>();
  FRONT_LOG(INFO) << LOG_BADGE("FrontService") << LOG_KV("this", this);
}

FrontService::~FrontService() {
  stop();
  FRONT_LOG(INFO) << LOG_BADGE("~FrontService") << LOG_KV("this", this);
}

// check the startup parameters, exception will be thrown if the required
// parameters are not set properly
void FrontService::checkParams() {
  if (m_groupID.empty()) {
    BOOST_THROW_EXCEPTION(InvalidParameter()
                          << errinfo_comment(" FrontService groupID not set"));
  }

  if (!m_nodeID) {
    BOOST_THROW_EXCEPTION(InvalidParameter()
                          << errinfo_comment(" FrontService nodeID not set"));
  }

  if (!m_gatewayInterface) {
    BOOST_THROW_EXCEPTION(InvalidParameter() << errinfo_comment(
                              " FrontService gatewayInterface nodeID not set"));
  }

  if (!m_messageFactory) {
    BOOST_THROW_EXCEPTION(InvalidParameter() << errinfo_comment(
                              " FrontService messageFactory not set"));
  }

  if (!m_ioService) {
    BOOST_THROW_EXCEPTION(InvalidParameter() << errinfo_comment(
                              " FrontService ioService not set"));
  }

  return;
}

void FrontService::start() {
  if (m_run) {
    FRONT_LOG(INFO) << LOG_BADGE("start")
                    << LOG_DESC("start service is running")
                    << LOG_KV("nodeID", m_nodeID->hex())
                    << LOG_KV("groupID", m_groupID);
    return;
  }

  checkParams();

  m_run = true;

  m_frontServiceThread = std::make_shared<std::thread>([=]() {
    while (m_run) {
      try {
        m_ioService->run();
      } catch (std::exception &e) {
        FRONT_LOG(WARNING) << LOG_DESC("IO Service exception")
                           << LOG_KV("what", boost::diagnostic_information(e));
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      if (m_run && m_ioService->stopped()) {
        m_ioService->restart();
      }
    }
  });

  FRONT_LOG(INFO) << LOG_BADGE("start") << LOG_KV("nodeID", m_nodeID->hex())
                  << LOG_KV("groupID", m_groupID);

  return;
}
void FrontService::stop() {
  if (!m_run) {
    return;
  }

  m_run = false;

  try {
    if (m_run && m_ioService->stopped()) {
      m_ioService->reset();
    }

    if (m_threadPool) {
      m_threadPool->stop();
    }

    if (m_frontServiceThread && m_frontServiceThread->joinable()) {
      m_frontServiceThread->join();
    }

    // handle callbacks
    auto errorPtr =
        std::make_shared<Error>(CommonError::TIMEOUT, "front service stopped");

    {
      RecursiveGuard l(x_callback);
      for (auto &callback : m_callback) {
        FRONT_LOG(INFO) << LOG_DESC("front service stopped")
                        << LOG_KV("message uuid", callback.first);
        // cancel the timer first
        callback.second->timeoutHandler->cancel();
        callback.second->callbackFunc(errorPtr, nullptr, bytesConstRef(),
                                      std::function<void(bytesConstRef)>());
      }
    }

  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
  }

  FRONT_LOG(INFO) << LOG_BADGE("stop end") << LOG_KV("nodeID", m_nodeID->hex())
                  << LOG_KV("groupID", m_groupID);

  return;
}

/**
 * @brief: get nodeID list
 * @return void
 */
void FrontService::asyncGetNodeIDs(
    std::function<void(Error::Ptr _error,
                       std::shared_ptr<const crypto::NodeIDs> _nodeIDs)>
        _callback) const {
  auto errorPtr = std::make_shared<Error>(CommonError::SUCCESS, "success");
  {
    Guard l(x_nodeIDs);
    _callback(errorPtr, m_nodeIDs);
  }
}

/**
 * @brief: send message to node
 * @param _moduleID: moduleID
 * @param _nodeID: the receiver nodeID
 * @param _data: send data
 * @param _timeout: the timeout value of async function, in milliseconds.
 * @param _callbackFunc: _callbackFunc
 * @return void
 */
void FrontService::asyncSendMessageByNodeID(int _moduleID,
                                            bcos::crypto::NodeIDPtr _nodeID,
                                            bytesConstRef _data,
                                            uint32_t _timeout,
                                            CallbackFunc _callbackFunc) {

  try {
    std::string uuid;

    if (_callbackFunc) {
      uuid = boost::uuids::to_string(boost::uuids::random_generator()());

      auto callback = std::make_shared<Callback>();
      callback->callbackFunc = _callbackFunc;

      if (_timeout > 0) {
        // create new timer to handle timeout
        auto timeoutHandler = std::make_shared<boost::asio::deadline_timer>(
            *m_ioService, boost::posix_time::milliseconds(_timeout));

        callback->timeoutHandler = timeoutHandler;
        // callback->startTime = utcSteadyTime();
        timeoutHandler->async_wait([frontServicePtr = shared_from_this(),
                                    uuid](const boost::system::error_code &e) {
          frontServicePtr->onMessageTimeout(e, uuid);
        });
      }

      addCallback(uuid, callback);

      FRONT_LOG(DEBUG) << LOG_BADGE("asyncSendMessageByNodeID")
                       << LOG_KV("uuid", uuid) << LOG_KV("groupID", m_groupID)
                       << LOG_KV("nodeID", m_nodeID->hex())
                       << LOG_KV("timeout", _timeout);

    } // if (_callback)

    onSendMessage(_moduleID, _nodeID, uuid, _data);

  } catch (std::exception &e) {
    FRONT_LOG(ERROR) << LOG_BADGE("asyncSendMessageByNodeID")
                     << LOG_KV("what", boost::diagnostic_information(e));
  }
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

  FRONT_LOG(INFO) << LOG_BADGE("registerNodeStatusNotifier")
                  << LOG_KV("moduleID", _moduleID)
                  << LOG_KV("nodeID", m_nodeID->hex())
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

  FRONT_LOG(INFO) << LOG_BADGE("registerMessageDispatcher")
                  << LOG_KV("moduleID", _moduleID)
                  << LOG_KV("nodeID", m_nodeID->hex())
                  << LOG_KV("groupID", m_groupID);
}

/**
 * @brief: receive nodeIDs from gateway
 * @param _error: error info
 * @param _nodeIDs: received nodeIDs
 * @return void
 */
void FrontService::onReceiveNodeIDs(
    Error::Ptr _error, std::shared_ptr<const bcos::crypto::NodeIDs> _nodeIDs) {

  // error
  if (_error && _error->errorCode() != CommonError::SUCCESS) {
    FRONT_LOG(ERROR) << LOG_BADGE("onReceiveNodeIDs")
                     << LOG_KV("errorCode", _error->errorCode())
                     << LOG_KV("errorMessage", _error->errorMessage());
    return;
  }

  {
    Guard l(x_nodeIDs);
    m_nodeIDs = _nodeIDs;
  }

  // notify all modules that registered to fetch nodeIDs
  auto errorPtr = std::make_shared<Error>(CommonError::SUCCESS, "success");
  for (const auto &notifier : m_mapNodeStatusNotifier) {
    notifier.second(errorPtr);
  }
}

/**
 * @brief: receive message from gateway
 * @param _error: error info
 * @param _nodeID: the node send this message
 * @param _data: received message data
 * @return void
 */
void FrontService::onReceiveMessage(Error::Ptr _error,
                                    bcos::crypto::NodeIDPtr _nodeID,
                                    bytesConstRef _data) {

  // error
  if (_error && _error->errorCode() != CommonError::SUCCESS) {
    FRONT_LOG(ERROR) << LOG_BADGE("onReceiveMessage")
                     << LOG_KV("errorCode", _error->errorCode())
                     << LOG_KV("errorMessage", _error->errorMessage());
    return;
  }

  try {
    auto message = messageFactory()->buildMessage();
    auto result = message->decode(_data);
    if (MessageDecodeStatus::MESSAGE_COMPLETE != result) {
      FRONT_LOG(ERROR) << LOG_BADGE("onReceiveMessage")
                       << LOG_DESC("invalid message format")
                       << LOG_KV("messageLength", _data.size())
                       << LOG_KV("fromNodeID", m_nodeID->hex());
      return;
    }

    int moduleID = message->moduleID();
    int ext = message->ext();
    std::string uuid =
        std::string(message->uuid()->begin(), message->uuid()->end());

    FRONT_LOG(DEBUG) << LOG_BADGE("onReceiveMessage")
                     << LOG_DESC("receive front pacaket")
                     << LOG_KV("moduleID", moduleID) << LOG_KV("uuid", uuid)
                     << LOG_KV("ext", ext) << LOG_KV("groupID", m_groupID)
                     << LOG_KV("fromNodeID", m_nodeID->hex())
                     << LOG_KV("dataSize", _data.size());

    // response func
    auto _respFunc = std::bind(&FrontService::onSendMessage, shared_from_this(),
                               moduleID, _nodeID, uuid, std::placeholders::_1);

    auto errorPtr = std::make_shared<Error>(CommonError::SUCCESS, "success");
    if (!uuid.empty()) {
      // check if callback message
      auto callback = getAndRemoveCallback(uuid);
      if (callback) {
        // cancel the timer first
        callback->timeoutHandler->cancel();
        if (m_threadPool) {
          // construct shared_ptr<bytes> from message->payload() first for
          // thead safe
          std::shared_ptr<bytes> buffer = std::make_shared<bytes>(
              message->payload().begin(), message->payload().end());

          m_threadPool->enqueue(
              [callback, buffer, errorPtr, _nodeID, _respFunc] {
                callback->callbackFunc(
                    errorPtr, _nodeID,
                    bytesConstRef(buffer->data(), buffer->size()), _respFunc);
              });
        } else {
          callback->callbackFunc(errorPtr, _nodeID, message->payload(),
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
        // construct shared_ptr<bytes> from message->payload() first for thead
        // safe
        std::shared_ptr<bytes> buffer = std::make_shared<bytes>(
            message->payload().begin(), message->payload().end());

        m_threadPool->enqueue([buffer, errorPtr, _nodeID, _respFunc,
                               moduleCallback] {
          moduleCallback->second(errorPtr, _nodeID,
                                 bytesConstRef(buffer->data(), buffer->size()),
                                 _respFunc);
        });
      } else {
        moduleCallback->second(errorPtr, _nodeID, message->payload(),
                               _respFunc);
      }
    } else {
      FRONT_LOG(WARNING) << LOG_BADGE("onReceiveMessage")
                         << LOG_DESC("found no module callback")
                         << LOG_KV("moduleID", moduleID)
                         << LOG_KV("uuid", uuid);
    }
  } catch (std::exception &e) {
    FRONT_LOG(ERROR) << "onReceiveMessage exception"
                     << LOG_KV("what", boost::diagnostic_information(e));
  }
}

/**
 * @brief: send message, call by asyncSendMessageByNodeID
 * @param _moduleID: moduleID
 * @param _nodeID: the node where the message needs to be sent to
 * @param _uuid: unique ID to identify this message
 * @param _data: send data
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
  auto ret = message->encode(*buffer.get());

  // call gateway interface to send the message
  m_gatewayInterface->asyncSendMessageByNodeID(
      m_groupID, _nodeID, bytesConstRef(buffer->data(), buffer->size()), 0,
      gateway::CallbackFunc());

  FRONT_LOG(TRACE) << LOG_BADGE("onSendMessage") << LOG_KV("ret", ret)
                   << LOG_KV("moduleID", _moduleID)
                   << LOG_KV("nodeID", _nodeID->hex()) << LOG_KV("uuid", _uuid)
                   << LOG_KV("bufferSize", buffer->size())
                   << LOG_KV("uuid", _uuid);
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
    FRONT_LOG(ERROR) << LOG_BADGE("onMessageTimeout")
                     << LOG_DESC("async_await error") << LOG_KV("error", _error)
                     << LOG_KV("uuid", _uuid);
    return;
  }

  try {
    Callback::Ptr callback = getAndRemoveCallback(_uuid);
    if (callback) {
      auto errorPtr =
          std::make_shared<Error>(CommonError::TIMEOUT, "message timeout");
      callback->callbackFunc(errorPtr, nullptr, bytesConstRef(),
                             std::function<void(bytesConstRef)>());
    }

    FRONT_LOG(WARNING) << LOG_BADGE("onMessageTimeout")
                       << LOG_KV("uuid", _uuid);

  } catch (std::exception &e) {
    FRONT_LOG(ERROR) << "onMessageTimeout exception" << LOG_KV("uuid", _uuid)
                     << LOG_KV("what", boost::diagnostic_information(e));
  }
}

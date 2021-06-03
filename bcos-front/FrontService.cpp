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
#include <thread>

#include <bcos-front/Common.h>
#include <bcos-front/FrontMessage.h>
#include <bcos-front/FrontService.h>
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
  FRONT_LOG(INFO) << LOG_DESC("FrontService") << LOG_KV("this", this);
}

FrontService::~FrontService() {
  stop();
  FRONT_LOG(INFO) << LOG_DESC("~FrontService") << LOG_KV("this", this);
}

// check the startup parameters, exception will be thrown if the required
// parameters are not set properly
void FrontService::checkParams() {
  if (m_groupID.empty()) {
    BOOST_THROW_EXCEPTION(InvalidParameter() << errinfo_comment(
                              " FrontService groupID is uninitialized"));
  }

  if (!m_nodeID) {
    BOOST_THROW_EXCEPTION(InvalidParameter() << errinfo_comment(
                              " FrontService nodeID is uninitialized"));
  }

  if (!m_gatewayInterface) {
    BOOST_THROW_EXCEPTION(
        InvalidParameter() << errinfo_comment(
            " FrontService gatewayInterface is uninitialized"));
  }

  if (!m_messageFactory) {
    BOOST_THROW_EXCEPTION(InvalidParameter() << errinfo_comment(
                              " FrontService messageFactory is uninitialized"));
  }

  if (!m_ioService) {
    BOOST_THROW_EXCEPTION(InvalidParameter() << errinfo_comment(
                              " FrontService ioService is uninitialized"));
  }

  return;
}

void FrontService::start() {
  if (m_run) {
    FRONT_LOG(INFO) << LOG_BADGE("start")
                    << LOG_DESC("front service is running")
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
        FRONT_LOG(WARNING) << LOG_DESC("IOService")
                           << LOG_KV("error", boost::diagnostic_information(e));
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      if (m_run && m_ioService->stopped()) {
        m_ioService->restart();
      }
    }
  });

  FRONT_LOG(INFO) << LOG_DESC("start") << LOG_KV("nodeID", m_nodeID->hex())
                  << LOG_KV("groupID", m_groupID);

  FRONT_LOG(INFO) << LOG_DESC("register module")
                  << LOG_KV("count", m_moduleID2MessageDispatcher.size());
  for (const auto &module : m_moduleID2MessageDispatcher) {
    FRONT_LOG(INFO) << LOG_DESC("register module")
                    << LOG_KV("moduleID", module.first);
  }

  return;
}
void FrontService::stop() {
  if (!m_run) {
    return;
  }

  m_run = false;

  try {
    if (m_ioService && m_ioService->stopped()) {
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
        FRONT_LOG(INFO) << LOG_DESC("FrontService stopped")
                        << LOG_KV("uuid", callback.first);
        // cancel the timer
        if (callback.second->timeoutHandler) {
          callback.second->timeoutHandler->cancel();
        }
        callback.second->callbackFunc(errorPtr, nullptr, bytesConstRef(),
                                      std::string(""),
                                      std::function<void(bytesConstRef)>());
      }
    }

  } catch (const std::exception &e) {
    FRONT_LOG(ERROR) << LOG_DESC("FrontService stop")
                     << LOG_KV("error", boost::diagnostic_information(e));
  }

  FRONT_LOG(INFO) << LOG_DESC("FrontService stop")
                  << LOG_KV("nodeID", (m_nodeID ? m_nodeID->hex() : ""))
                  << LOG_KV("groupID", m_groupID);

  return;
}

/**
 * @brief: get nodeIDs from frontservice
 * @param _getNodeIDsFunc: response callback
 * @return void
 */
void FrontService::asyncGetNodeIDs(GetNodeIDsFunc _getNodeIDsFunc) {
  std::shared_ptr<const crypto::NodeIDs> nodeIDs;
  {
    Guard l(x_nodeIDs);
    nodeIDs = m_nodeIDs;
  }

  auto okPtr = std::make_shared<Error>(CommonError::SUCCESS, "success");
  if (_getNodeIDsFunc) {
    if (m_threadPool) {
      m_threadPool->enqueue([okPtr, nodeIDs, _getNodeIDsFunc]() {
        _getNodeIDsFunc(okPtr, nodeIDs);
      });
    } else {
      _getNodeIDsFunc(okPtr, nodeIDs);
    }
  }

  FRONT_LOG(INFO) << LOG_DESC("asyncGetNodeIDs")
                  << LOG_KV("nodeIDs.size()", nodeIDs->size());

  return;
}

/**
 * @brief: send message
 * @param _moduleID: moduleID
 * @param _nodeID: the receiver nodeID
 * @param _data: send message data
 * @param _timeout: timeout, in milliseconds.
 * @param _callbackFunc: callback
 * @return void
 */
void FrontService::asyncSendMessageByNodeID(int _moduleID,
                                            bcos::crypto::NodeIDPtr _nodeID,
                                            bytesConstRef _data,
                                            uint32_t _timeout,
                                            CallbackFunc _callbackFunc) {
  try {
    std::string uuid =
        boost::uuids::to_string(boost::uuids::random_generator()());
    if (_callbackFunc) {
      auto callback = std::make_shared<Callback>();
      callback->callbackFunc = _callbackFunc;

      if (_timeout > 0) {
        // create new timer to handle timeout
        auto timeoutHandler = std::make_shared<boost::asio::deadline_timer>(
            *m_ioService, boost::posix_time::milliseconds(_timeout));

        callback->timeoutHandler = timeoutHandler;
        auto frontServiceWeakPtr =
            std::weak_ptr<FrontService>(shared_from_this());
        // callback->startTime = utcSteadyTime();
        timeoutHandler->async_wait(
            [frontServiceWeakPtr, uuid](const boost::system::error_code &e) {
              auto frontService = frontServiceWeakPtr.lock();
              if (frontService) {
                frontService->onMessageTimeout(e, uuid);
              }
            });
      }

      addCallback(uuid, callback);

      FRONT_LOG(DEBUG) << LOG_DESC("asyncSendMessageByNodeID")
                       << LOG_KV("groupID", m_groupID)
                       << LOG_KV("moduleID", _moduleID) << LOG_KV("uuid", uuid)
                       << LOG_KV("nodeID", _nodeID->hex())
                       << LOG_KV("data.size()", _data.size())
                       << LOG_KV("timeout", _timeout);

    } // if (_callback)

    sendMessage(_moduleID, _nodeID, uuid, _data);

  } catch (std::exception &e) {
    FRONT_LOG(ERROR) << LOG_BADGE("asyncSendMessageByNodeID")
                     << LOG_KV("error", boost::diagnostic_information(e));
  }
}

/**
 * @brief: send response
 * @param _id: the request uuid
 * @param _data: message
 * @return void
 */
void FrontService::asyncSendResponse(const std::string &_id,
                                     bytesConstRef _data) {
  // There is no logic, the implementation in the tars
  (void)_id;
  (void)_data;
}

/**
 * @brief: send message to multiple nodes
 * @param _moduleID: moduleID
 * @param _nodeIDs: the receiver nodeIDs
 * @param _data: send message data
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
 * @param _data: send message data
 * @return void
 */
void FrontService::asyncSendBroadcastMessage(int _moduleID,
                                             bytesConstRef _data) {

  auto message = messageFactory()->buildMessage();
  message->setModuleID(_moduleID);
  message->setPayload(_data);

  auto buffer = std::make_shared<bytes>();
  message->encode(*buffer.get());

  m_gatewayInterface->asyncSendBroadcastMessage(
      m_groupID, m_nodeID, bytesConstRef(buffer->data(), buffer->size()));
}

/**
 * @brief: receive nodeIDs from gateway
 * @param _groupID: groupID
 * @param _nodeIDs: nodeIDs pushed by gateway
 * @param _receiveMsgCallback: response callback
 * @return void
 */
void FrontService::onReceiveNodeIDs(
    const std::string &_groupID,
    std::shared_ptr<const crypto::NodeIDs> _nodeIDs,
    ReceiveMsgFunc _receiveMsgCallback) {
  {
    Guard l(x_nodeIDs);
    m_nodeIDs = _nodeIDs;
  }

  FRONT_LOG(INFO) << LOG_DESC("onReceiveNodeIDs") << LOG_KV("groupID", _groupID)
                  << LOG_KV("nodeIDs.size()", _nodeIDs->size());

  if (_receiveMsgCallback) {
    auto okPtr = std::make_shared<Error>(CommonError::SUCCESS, "success");
    if (m_threadPool) {
      m_threadPool->enqueue(
          [okPtr, _receiveMsgCallback]() { _receiveMsgCallback(okPtr); });
    } else {
      _receiveMsgCallback(okPtr);
    }
  }
}

/**
 * @brief: receive message from gateway
 * @param _groupID: groupID
 * @param _nodeID: the node send the message
 * @param _data: received message data
 * @param _receiveMsgCallback: response callback
 * @return void
 */
void FrontService::onReceiveMessage(const std::string &_groupID,
                                    bcos::crypto::NodeIDPtr _nodeID,
                                    bytesConstRef _data,
                                    ReceiveMsgFunc _receiveMsgCallback) {
  try {
    auto message = messageFactory()->buildMessage();
    auto ret = message->decode(_data);
    if (MessageDecodeStatus::MESSAGE_COMPLETE != ret) {
      FRONT_LOG(ERROR) << LOG_DESC("onReceiveMessage")
                       << LOG_DESC("illegal message")
                       << LOG_KV("length", _data.size())
                       << LOG_KV("nodeID", m_nodeID->hex());
      BOOST_THROW_EXCEPTION(InvalidParameter()
                            << errinfo_comment("illegal message"));
    }

    int moduleID = message->moduleID();
    int ext = message->ext();
    std::string uuid =
        std::string(message->uuid()->begin(), message->uuid()->end());

    FRONT_LOG(TRACE) << LOG_BADGE("onReceiveMessage")
                     << LOG_KV("moduleID", moduleID) << LOG_KV("uuid", uuid)
                     << LOG_KV("ext", ext) << LOG_KV("groupID", _groupID)
                     << LOG_KV("nodeID", _nodeID->hex())
                     << LOG_KV("length", _data.size());

    auto frontServiceWeakPtr = std::weak_ptr<FrontService>(shared_from_this());
    auto _respFunc = [frontServiceWeakPtr, moduleID, _nodeID,
                      uuid](bytesConstRef _data) {
      auto frontService = frontServiceWeakPtr.lock();
      if (frontService) {
        frontService->sendMessage(moduleID, _nodeID, uuid, _data, true);
      }
    };

    auto okPtr = std::make_shared<Error>(CommonError::SUCCESS, "success");
    if (message->isResponse()) {
      // callback message
      auto callback = getAndRemoveCallback(uuid);
      if (callback) {
        // cancel the timer first
        if (callback->timeoutHandler) {
          callback->timeoutHandler->cancel();
        }

        if (m_threadPool) {
          // construct shared_ptr<bytes> from message->payload() first for
          // thead safe
          std::shared_ptr<bytes> buffer = std::make_shared<bytes>(
              message->payload().begin(), message->payload().end());
          m_threadPool->enqueue([uuid, callback, buffer, okPtr, _nodeID,
                                 _respFunc] {
            callback->callbackFunc(
                okPtr, _nodeID, bytesConstRef(buffer->data(), buffer->size()),
                uuid, _respFunc);
          });
        } else {
          callback->callbackFunc(okPtr, _nodeID, message->payload(), uuid,
                                 _respFunc);
        }
      } else {
        FRONT_LOG(DEBUG) << LOG_DESC("unable find the resp callback")
                         << LOG_KV("uuid", uuid);
      }
    } else {
      auto it = m_moduleID2MessageDispatcher.find(moduleID);
      if (it != m_moduleID2MessageDispatcher.end()) {
        if (m_threadPool) {
          auto callback = it->second;
          // construct shared_ptr<bytes> from message->payload() first for
          // thead safe
          std::shared_ptr<bytes> buffer = std::make_shared<bytes>(
              message->payload().begin(), message->payload().end());
          m_threadPool->enqueue([callback, buffer, message, _nodeID] {
            callback(_nodeID, bytesConstRef(buffer->data(), buffer->size()));
          });
        } else {
          it->second(_nodeID, message->payload());
        }
      } else {
        FRONT_LOG(WARNING)
            << LOG_DESC("unable find the register module message dispather")
            << LOG_KV("moduleID", moduleID) << LOG_KV("uuid", uuid);
      }
    }
  } catch (const std::exception &e) {
    FRONT_LOG(ERROR) << "onReceiveMessage"
                     << LOG_KV("error", boost::diagnostic_information(e));
  }

  if (_receiveMsgCallback) {
    auto okPtr = std::make_shared<Error>(CommonError::SUCCESS, "success");
    if (m_threadPool) {
      m_threadPool->enqueue(
          [okPtr, _receiveMsgCallback]() { _receiveMsgCallback(okPtr); });
    } else {
      _receiveMsgCallback(okPtr);
    }
  }
}

/**
 * @brief: receive broadcast message from gateway
 * @param _groupID: groupID
 * @param _nodeID: the node send the message
 * @param _data: received message data
 * @param _receiveMsgCallback: response callback
 * @return void
 */
void FrontService::onReceiveBroadcastMessage(
    const std::string &_groupID, bcos::crypto::NodeIDPtr _nodeID,
    bytesConstRef _data, ReceiveMsgFunc _receiveMsgCallback) {
  onReceiveMessage(_groupID, _nodeID, _data, _receiveMsgCallback);
}

/**
 * @brief: send message
 * @param _moduleID: moduleID
 * @param _nodeID: the node the message sent to
 * @param _uuid: uuid identify this message
 * @param _data: send data payload
 * @return void
 */
void FrontService::sendMessage(int _moduleID, bcos::crypto::NodeIDPtr _nodeID,
                               const std::string &_uuid, bytesConstRef _data,
                               bool resp) {

  auto message = messageFactory()->buildMessage();
  message->setModuleID(_moduleID);
  message->setUuid(std::make_shared<bytes>(_uuid.begin(), _uuid.end()));
  message->setPayload(_data);
  if (resp) {
    message->setResponse();
  }

  auto buffer = std::make_shared<bytes>();
  message->encode(*buffer.get());

  // call gateway interface to send the message
  m_gatewayInterface->asyncSendMessageByNodeID(
      m_groupID, m_nodeID, _nodeID,
      bytesConstRef(buffer->data(), buffer->size()),
      [_moduleID, _nodeID, _uuid](Error::Ptr _error) {
        if (_error && (_error->errorCode() != CommonError::SUCCESS))
          FRONT_LOG(WARNING)
              << LOG_DESC("sendMessage response errorCode")
              << LOG_KV("errorCode", _error->errorCode())
              << LOG_KV("moduleID", _moduleID)
              << LOG_KV("nodeID", _nodeID->hex()) << LOG_KV("uuid", _uuid);
      });
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
    FRONT_LOG(ERROR) << LOG_DESC("onMessageTimeout") << LOG_KV("error", _error)
                     << LOG_KV("uuid", _uuid);
    return;
  }

  try {
    Callback::Ptr callback = getAndRemoveCallback(_uuid);
    if (callback) {
      auto errorPtr = std::make_shared<Error>(CommonError::TIMEOUT, "timeout");
      if (m_threadPool) {
        m_threadPool->enqueue([_uuid, callback, errorPtr]() {
          callback->callbackFunc(errorPtr, nullptr, bytesConstRef(), _uuid,
                                 std::function<void(bytesConstRef)>());
        });
      } else {
        callback->callbackFunc(errorPtr, nullptr, bytesConstRef(), _uuid,
                               std::function<void(bytesConstRef)>());
      }
    }

    FRONT_LOG(DEBUG) << LOG_BADGE("onMessageTimeout") << LOG_KV("uuid", _uuid);
  } catch (std::exception &e) {
    FRONT_LOG(ERROR) << "onMessageTimeout" << LOG_KV("uuid", _uuid)
                     << LOG_KV("error", boost::diagnostic_information(e));
  }
}
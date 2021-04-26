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
 * @file FrontService.h
 * @author: octopus
 * @date 2021-04-19
 */

#pragma once

#include "FrontMessage.h"
#include <bcos-framework/interfaces/front/FrontServiceInterface.h>
#include <bcos-framework/interfaces/gateway/GatewayInterface.h>
#include <bcos-framework/libutilities/Common.h>
#include <bcos-framework/libutilities/ThreadPool.h>
#include <boost/asio.hpp>

namespace bcos {
namespace front {

class FrontService : public FrontServiceInterface,
                     public std::enable_shared_from_this<FrontService> {
public:
  using Ptr = std::shared_ptr<FrontService>;

  FrontService();
  virtual ~FrontService();

  virtual void start();
  virtual void stop();

public:
  /**
   * @brief: get nodeID list
   * @return void
   */
  virtual void asyncGetNodeIDs(
      std::function<void(Error::Ptr _error,
                         const std::shared_ptr<const NodeIDs> &)>)
      const override;

  /**
   * @brief: send message to node
   * @param _moduleID: moduleID
   * @param _nodeID: the receiver nodeID
   * @param _data: send data
   * @param _timeout: the timeout value of async function, in milliseconds.
   * @param _callback: callback
   * @return void
   */
  virtual void asyncSendMessageByNodeID(int _moduleID,
                                        bcos::crypto::NodeIDPtr _nodeID,
                                        bytesConstRef _data, uint32_t _timeout,
                                        CallbackFunc _callback) override;

  /**
   * @brief: send message to multiple nodes
   * @param _moduleID: moduleID
   * @param _nodeIDs: the receiver nodeIDs
   * @param _data: send data
   * @return void
   */
  virtual void asyncSendMessageByNodeIDs(int _moduleID, const NodeIDs &_nodeIDs,
                                         bytesConstRef _data) override;

  /**
   * @brief: send broadcast message
   * @param _moduleID: moduleID
   * @param _data:  send data
   * @return void
   */
  virtual void asyncMulticastMessage(int _moduleID,
                                     bytesConstRef _data) override;

  /**
   * @brief: register the node change callback
   * @param _moduleID: moduleID
   * @param _notifier: callback
   * @return void
   */
  virtual void
  registerNodeStatusNotifier(int _moduleID,
                             NodeStatusNotifier _notifier) override;

  /**
   * @brief: register the callback for module message
   * @param _moduleID: moduleID
   * @param _dispatcher: callback
   * @return void
   */
  virtual void
  registerMessageDispatcher(int _moduleID,
                            MessageDispatcher _dispatcher) override;

  /**
   * @brief: receive message from gateway
   * @param _nodeID: the node send this message
   * @param _data: send data
   * @return void
   */
  virtual void onReceiveMessage(bcos::crypto::NodeIDPtr _nodeID,
                                bytesConstRef _data);

  /**
   * @brief: send message, call by asyncSendMessageByNodeID
   * @param _moduleID: moduleID
   * @param _nodeID: the node where the message needs to be sent to
   * @param _uuid: unique ID to identify this message
   * @param _data: send data
   * @return void
   */
  void onSendMessage(int _moduleID, bcos::crypto::NodeIDPtr _nodeID,
                     const std::string &_uuid, bytesConstRef _data);

  /**
   * @brief: handle message timeout
   * @param _error: boost error code
   * @param _uuid: message uuid
   * @return void
   */
  void onMessageTimeout(const boost::system::error_code &_error,
                        const std::string &_uuid);

public:
  FrontMessageFactory::Ptr messageFactory() const { return m_messageFactory; }

  void setMessageFactory(FrontMessageFactory::Ptr _messageFactory) {
    m_messageFactory = _messageFactory;
  }

  const std::string &stringNodeID() const { return m_stringNodeID; }

  bcos::crypto::NodeIDPtr nodeID() const { return m_nodeID; }
  void setNodeID(bcos::crypto::NodeIDPtr _nodeID) {
    m_nodeID = _nodeID;
    m_stringNodeID =
        std::string(m_nodeID->data().begin(), m_nodeID->data().end());
  }
  std::string groupID() const { return m_groupID; }
  void setGroupID(const std::string &_groupID) { m_groupID = _groupID; }

  std::shared_ptr<gateway::GatewayInterface> gatewayInterface() {
    return m_gatewayInterface;
  }

  void setGatewayInterface(
      std::shared_ptr<gateway::GatewayInterface> _gatewayInterface) {
    m_gatewayInterface = _gatewayInterface;
  }

  std::shared_ptr<boost::asio::io_service> ioService() const {
    return m_ioService;
  }
  void setIoService(std::shared_ptr<boost::asio::io_service> _ioService) {
    m_ioService = _ioService;
  }

  bcos::ThreadPool::Ptr threadPool() const { return m_threadPool; }
  void setThreadPool(bcos::ThreadPool::Ptr _threadPool) {
    m_threadPool = _threadPool;
  }

private:
  struct Callback : public std::enable_shared_from_this<Callback> {
    using Ptr = std::shared_ptr<Callback>;
    uint64_t startTime = utcSteadyTime();
    CallbackFunc callbackFunc;
    std::shared_ptr<boost::asio::deadline_timer> timeoutHandler;
  };
  // lock m_callback
  mutable bcos::RecursiveMutex x_callback;
  // uuid to callback
  std::unordered_map<std::string, Callback::Ptr> m_callback;

  Callback::Ptr getAndRemoveCallback(const std::string &_uuid) {
    Callback::Ptr callback = nullptr;

    RecursiveGuard l(x_callback);
    auto it = m_callback.find(_uuid);
    if (it != m_callback.end()) {
      callback = it->second;
      m_callback.erase(it);
    }

    return callback;
  }

  void addCallback(const std::string &_uuid, Callback::Ptr _callback) {
    RecursiveGuard l(x_callback);
    m_callback[_uuid] = _callback;
  }

private:
  // thread pool
  bcos::ThreadPool::Ptr m_threadPool;
  // timer
  std::shared_ptr<boost::asio::io_service> m_ioService;
  /// gateway interface
  std::shared_ptr<bcos::gateway::GatewayInterface> m_gatewayInterface;

  FrontMessageFactory::Ptr m_messageFactory;
  std::unordered_map<int, MessageDispatcher> m_mapMessageDispatcher;
  std::unordered_map<int, NodeStatusNotifier> m_mapNodeStatusNotifier;

  // ervice is running or not
  bool m_run = false;
  // NodeID
  bcos::crypto::NodeIDPtr m_nodeID;
  // NodeID in string format
  std::string m_stringNodeID;
  // GroupID
  std::string m_groupID;
};
} // namespace front
} // namespace bcos

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
 * @brief Gateway fake implementation
 * @file FakeGateway.h
 * @author: octopus
 * @date 2021-04-27
 */

#pragma once

#include <bcos-framework/interfaces/front/FrontServiceInterface.h>
#include <bcos-framework/interfaces/gateway/GatewayInterface.h>
#include <bcos-framework/libutilities/Common.h>
#include <bcos-framework/libutilities/ThreadPool.h>
#include <boost/asio.hpp>

namespace bcos {
namespace front {
namespace test {

class FakeGateway : public gateway::GatewayInterface,
                    public std::enable_shared_from_this<FakeGateway> {

private:
  // parameters
  std::string m_groupID;
  bcos::crypto::NodeIDPtr m_nodeID;
  std::shared_ptr<bytes> m_payload;

public:
  const std::string groupID() const { return m_groupID; }
  bcos::crypto::NodeIDPtr nodeID() const { return m_nodeID; }
  std::shared_ptr<bytes> payload() const { return m_payload; }

public:
  FakeGateway() {}
  virtual ~FakeGateway() {}
  /**
   * @brief:
   * @param _groupID : groupID
   * @param _nodeID: nodeID
   * @param _messageCallback: callback
   * @return void
   */
  virtual void registerFrontMessageCallback(
      const std::string &_groupID, bcos::crypto::NodeIDPtr _nodeID,
      bcos::gateway::CallbackFunc _messageCallback) override;

  /**
   * @brief:
   * @param _groupID : groupID
   * @param _nodeID: nodeID
   * @param _nodeStatusCallback: callback
   * @return void
   */
  virtual void registerNodeStatusNotifier(
      const std::string &_groupID, bcos::crypto::NodeIDPtr _nodeID,
      std::function<void(Error::Ptr _error)> _nodeStatusCallback) override;

  /**
   * @brief: get nodeID list
   * @return void
   */
  virtual void asyncGetNodeIDs(
      std::function<
          void(Error::Ptr _error,
               std::shared_ptr<const std::vector<bcos::crypto::NodeIDPtr>> &)>)
      const override;

  /**
   * @brief: send message to a single node
   * @param _groupID: groupID
   * @param _nodeID: the receiver nodeID
   * @param _payload: message content
   * @param _options: option parameters
   * @param _callback: callback
   * @return void
   */
  virtual void
  asyncSendMessageByNodeID(const std::string &_groupID,
                           bcos::crypto::NodeIDPtr _nodeID,
                           bytesConstRef _payload, uint32_t _timeout,
                           bcos::gateway::CallbackFunc _callback) override;

  /**
   * @brief: send message to multiple nodes
   * @param _groupID: groupID
   * @param _nodeIDs: the receiver nodeIDs
   * @param _payload: message content
   * @return void
   */
  virtual void asyncSendMessageByNodeIDs(const std::string &_groupID,
                                         const bcos::crypto::NodeIDs &_nodeIDs,
                                         bytesConstRef _payload) override;

  /**
   * @brief: send message to all nodes
   * @param _groupID: groupID
   * @param _payload: message content
   * @return void
   */
  virtual void asyncMulticastMessage(const std::string &_groupID,
                                     bytesConstRef _payload) override;
};

} // namespace test
} // namespace front
} // namespace bcos

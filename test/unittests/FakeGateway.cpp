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
 * @file FakeGateway.cpp
 * @author: octopus
 * @date 2021-04-27
 */

#include "FakeGateway.h"
#include "front/Common.h"

using namespace bcos;
using namespace bcos::front;
using namespace bcos::front::test;
using namespace bcos::gateway;
/**
 * @brief:
 * @param _groupID : groupID
 * @param _nodeID: nodeID
 * @param _messageCallback: callback
 * @return void
 */
void FakeGateway::registerFrontMessageCallback(
    const std::string &_groupID, bcos::crypto::NodeIDPtr _nodeID,
    bcos::gateway::CallbackFunc _messageCallback) {
  (void)_groupID;
  (void)_nodeID;
  (void)_messageCallback;
}

/**
 * @brief:
 * @param _groupID : groupID
 * @param _nodeID: nodeID
 * @param _nodeStatusCallback: callback
 * @return void
 */
void FakeGateway::registerNodeStatusNotifier(
    const std::string &_groupID, bcos::crypto::NodeIDPtr _nodeID,
    std::function<void(Error::Ptr _error)> _nodeStatusCallback) {
  (void)_groupID;
  (void)_nodeID;
  (void)_nodeStatusCallback;
}

/**
 * @brief: get nodeID list
 * @return void
 */
void FakeGateway::asyncGetNodeIDs(
    std::function<void(
        Error::Ptr _error,
        std::shared_ptr<const std::vector<bcos::crypto::NodeIDPtr>> &)>) const {
}

/**
 * @brief: send message to a single node
 * @param _groupID: groupID
 * @param _nodeID: the receiver nodeID
 * @param _payload: message content
 * @param _options: option parameters
 * @param _callback: callback
 * @return void
 */
void FakeGateway::asyncSendMessageByNodeID(
    const std::string &_groupID, bcos::crypto::NodeIDPtr _nodeID,
    bytesConstRef _payload, uint32_t _timeout,
    bcos::gateway::CallbackFunc _callback) {

  (void)_timeout;
  (void)_callback;

  m_groupID = _groupID;
  m_nodeID = _nodeID;
  m_payload = std::make_shared<bytes>(_payload.begin(), _payload.end());

  FRONT_LOG(DEBUG) << "[FakeGateway] asyncSendMessageByNodeID"
                   << LOG_KV("groupID", _groupID)
                   << LOG_KV("nodeID", _nodeID->hex());
}

/**
 * @brief: send message to multiple nodes
 * @param _groupID: groupID
 * @param _nodeIDs: the receiver nodeIDs
 * @param _payload: message content
 * @return void
 */
void FakeGateway::asyncSendMessageByNodeIDs(
    const std::string &_groupID, const bcos::crypto::NodeIDs &_nodeIDs,
    bytesConstRef _payload) {
  (void)_nodeIDs;

  m_groupID = _groupID;
  m_payload = std::make_shared<bytes>(_payload.begin(), _payload.end());

  FRONT_LOG(DEBUG) << "[FakeGateway] asyncSendMessageByNodeIDs"
                   << LOG_KV("groupID", _groupID);
}

/**
 * @brief: send message to all nodes
 * @param _groupID: groupID
 * @param _payload: message content
 * @return void
 */
void FakeGateway::asyncMulticastMessage(const std::string &_groupID,
                                        bytesConstRef _payload) {
  m_groupID = _groupID;
  m_payload = std::make_shared<bytes>(_payload.begin(), _payload.end());

  FRONT_LOG(DEBUG) << "[FakeGateway] asyncMulticastMessage"
                   << LOG_KV("groupID", _groupID);
}

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

using namespace bcos;
using namespace front;

/**
 * @brief: get nodeID list
 * @return void
 */
void FrontService::asyncGetNodeIDs(
    std::function<void(Error::Ptr _error,
                       const std::shared_ptr<const std::vector<NodeID>> &)>)
    const override {}

/**
 * @brief: send message to node
 * @param _moduleID: moduleID
 * @param _nodeID: the receiver nodeID
 * @param _data: message
 * @param _timeout: the timeout value of async function, in milliseconds.
 * @param _callback: callback
 * @return void
 */
void FrontService::asyncSendMessageByNodeID(int _moduleID, NodeID _nodeID,
                                            bytesConstRef _data,
                                            uint32_t _timeout,
                                            CallbackFunc _callback) override {}

/**
 * @brief: send messages to multiple nodes
 * @param _moduleID: moduleID
 * @param _nodeIDs: the receiver nodeIDs
 * @param _data: message
 * @return void
 */
void FrontService::asyncSendMessageByNodeIDs(
    int _moduleID, const std::vector<NodeID> &_nodeIDs,
    bytesConstRef _data) override {}

/**
 * @brief: send broadcast message
 * @param _moduleID: moduleID
 * @param _data:  message
 * @return void
 */
void FrontService::asyncMulticastMessage(int _moduleID,
                                         bytesConstRef _data) override {}

/**
 * @brief: register the node change callback
 * @param _moduleID: moduleID
 * @param _callback: callback
 * @return void
 */
void FrontService::registerNodeStatusNotifier(
    int _moduleID, std::function<void(Error::Ptr _error)> _callback) override {}

/**
 * @brief: register the callback for module message
 * @param _moduleID: moduleID
 * @param _callback: callback
 * @return void
 */
void FrontService::registerMessageDispatcher(
    int _moduleID,
    std::function<void(Error::Ptr _error, const NodeID &_nodeID,
                       bytesConstRef _data,
                       std::function<void(bytesConstRef _respData)> _respFunc)>
        _callback) override{};
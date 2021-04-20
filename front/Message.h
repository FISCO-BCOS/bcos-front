
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
 * @file Message.h
 * @author: octopus
 * @date 2021-04-20
 */

#pragma once
#include <bcos-framework/libutilities/Common.h>

namespace bcos {
namespace front {

enum MessageDecodeStatus {
  MESSAGE_ERROR = -1, // error message packet
  MESSAGE_COMPLETE = 0
};

class Message : public std::enable_shared_from_this<Message> {
public:
  virtual ~Message(){};

  typedef std::shared_ptr<Message> Ptr;

  virtual void encode(bytes &_buffer) = 0;
  virtual ssize_t decode(const bytes &_buffer, size_t _size) = 0;
};

class MessageFactory : public std::enable_shared_from_this<MessageFactory> {
public:
  typedef std::shared_ptr<MessageFactory> Ptr;

  virtual ~MessageFactory(){};
  virtual Message::Ptr buildMessage() = 0;
};

} // namespace front
} // namespace bcos
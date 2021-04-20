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
 * @file FrontMessage.h
 * @author: octopus
 * @date 2021-04-20
 */

#pragma once
#include "Message.h"

namespace bcos {
namespace front {

/// moduleID          :2 bytes
/// UUID length       :1 bytes
/// UUID              :UUID length bytes
/// ext               :2 bytes
/// payload
class FrontMessage : public Message {
  const static size_t HEADER_MIN_LENGTH = 6;

public:
  FrontMessage() {
    m_uuid = std::make_shared<bytes>();
    m_payload = std::make_shared<bytes>();
  }

  virtual ~FrontMessage() {}

  typedef std::shared_ptr<FrontMessage> Ptr;

  virtual uint16_t moduleID() { return m_moduleID; }
  virtual uint16_t setModuleID(uint16_t _moduleID) { m_moduleID = _moduleID; }

  virtual uint8_t uuidLength() { return m_uuid; }
  virtual uint8_t setUuidLength(uint8_t _uuid) { m_uuid = _uuid; }

  virtual uint16_t ext() { return m_ext; }
  virtual uint16_t setExt(uint16_t _ext) { m_ext = _ext; }

  virtual std::shared_ptr<bytes> uuid() { return m_uuid; }
  virtual void setUuid(std::shared_ptr<bytes> _uuid) { m_uuid = _uuid; }

  virtual std::shared_ptr<bytes> payload() { return m_payload; }
  virtual void setPayload(std::shared_ptr<bytes> _payload) {
    m_payload = _payload;
  }

public:
  virtual void encode(bytes &_buffer) override;
  virtual ssize_t decode(const bytes &_buffer, size_t _size) override;

protected:
  uint16_t m_moduleID = 0;
  uint8_t m_uuidLength = 0;
  std::shared_ptr<bytes> m_uuid;
  uint16_t m_ext = 0;
  std::shared_ptr<bytes> m_payload; ///< message data
};

class FrontMessageFactory : public MessageFactory {
public:
  typedef std::shared_ptr<FrontMessageFactory> Ptr;

  virtual ~FrontMessageFactory() {}

  virtual Message::Ptr buildMessage() override {
    auto message = std::make_shared<FrontMessage>();
    return message;
  }
};

} // namespace front
} // namespace bcos

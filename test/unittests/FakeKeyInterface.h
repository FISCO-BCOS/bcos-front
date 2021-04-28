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

#include <bcos-framework/interfaces/crypto/KeyInterface.h>
#include <bcos-framework/libutilities/Common.h>

namespace bcos {
namespace front {
namespace test {
class FakeKeyInterface : public bcos::crypto::KeyInterface {

private:
  std::string m_content;
  bcos::bytes m_bytesContent;

public:
  FakeKeyInterface() = default;
  FakeKeyInterface(const std::string &_content) {
    m_content = _content;
    m_bytesContent.insert(m_bytesContent.end(), m_content.begin(),
                          m_content.end());
  }

  const std::string &content() const { return m_content; }
  const bcos::bytes &bytesContent() const { return m_bytesContent; }

  virtual ~FakeKeyInterface() {}
  virtual const bcos::bytes &data() const override { return m_bytesContent; }
  virtual size_t size() const override { return m_bytesContent.size(); }
  virtual char *mutableData() override { return NULL; }
  virtual const char *constData() const override { return NULL; }
  virtual std::shared_ptr<bcos::bytes> encode() const override {
    return nullptr;
  }
  virtual void decode(bcos::bytesConstRef _data) override { (void)_data; }
  virtual void decode(bcos::bytes &&_data) override { (void)_data; }

  virtual std::string shortHex() override { return m_content; }
  virtual std::string hex() override { return m_content; }
};

} // namespace test
} // namespace front
} // namespace bcos
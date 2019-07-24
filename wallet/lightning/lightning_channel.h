// Copyright 2019 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "core/lightning.h"
#include "wallet/wallet_db.h"
#include "wallet/common.h"

namespace beam::wallet::lightning
{
class LightningChannel : public Lightning::Channel
{
public:
    struct Codes
    {
        static const uint32_t Control0 = 1024 << 16;
        static const uint32_t MyWid = Control0 + 31;
    };
    LightningChannel(
            const std::shared_ptr<proto::FlyClient::INetwork>& net,
            const IWalletDB::Ptr& walletDB,
            proto::FlyClient::Request::IHandler& openHandler)
            : m_net(net), m_WalletDB(walletDB), m_openHandler(openHandler) {};
    LightningChannel(const LightningChannel&) = delete;
    void operator=(const LightningChannel&) = delete;
    // LightningChannel(LightningChannel&& channel) { m_net = std::move(channel.m_net);};
    // void operator=(LightningChannel&& channel) { m_net = std::move(channel.m_net);};
    ~LightningChannel();

    Height get_Tip() const override;
    proto::FlyClient::INetwork& get_Net() override;
    void get_Kdf(Key::IKdf::Ptr&) override;
    void AllocTxoID(Key::IDV&) override;
    Amount SelectInputs(
            std::vector<Key::IDV>& vInp, Amount valRequired) override;
    void SendPeer(Negotiator::Storage::Map&& dataOut) override;

    uintBig_t<16> m_ID;
    WalletID m_widTrg;
    std::shared_ptr<proto::FlyClient::INetwork> m_net;
    IWalletDB::Ptr m_WalletDB;
    bool m_SendMyWid = true;
    using FieldMap = std::map<uint32_t, ByteBuffer>;
    // void (*SendFunctor)(Request& r, Request::IHandler& h);
    proto::FlyClient::Request::IHandler& m_openHandler;
};
}  // namespace beam::wallet::lightning

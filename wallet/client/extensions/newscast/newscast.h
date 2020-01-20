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

#include "news_message.h"
#include "news_observer.h"
#include "newscast_protocol_parser.h"

#include "wallet/core/wallet.h"

using namespace beam::proto;

namespace beam::wallet
{
    /**
     *  Implementation of public news channels reader via bulletin board system (BBS).
     */
    class Newscast
        : public FlyClient::IBbsReceiver
    {
    public:
        Newscast(FlyClient::INetwork& network, NewscastProtocolParser& parser);

        /**
         *  FlyClient::IBbsReceiver implementation
         *  Executed to process BBS messages received on subscribed channels
         */
        virtual void OnMsg(proto::BbsMsg&& msg) override;
        
        // INewsObserver interface
        void Subscribe(INewsObserver* observer);
        void Unsubscribe(INewsObserver* observer);

        static constexpr BbsChannel BbsChannelsOffset = Bbs::s_MaxWalletChannels + 1024u;

    private:
		FlyClient::INetwork& m_network;                     /// source of incoming BBS messages
        NewscastProtocolParser& m_parser;                   /// news protocol parser
        std::vector<INewsObserver*> m_subscribers;          /// fresh news subscribers

        static const std::set<BbsChannel> m_channels;
        Timestamp m_lastTimestamp = getTimestamp() - 12*60*60;

        void notifySubscribers(NewsMessage msg) const;
    };

} // namespace beam::wallet
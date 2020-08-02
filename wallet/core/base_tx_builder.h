// Copyright 2018 The Beam Team
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

#include "common.h"
#include "wallet_db.h"
#include "base_transaction.h"

#include <condition_variable>
#include <boost/optional.hpp>
#include "utility/logger.h"

namespace beam::wallet
{
    class BaseTxBuilder
        :public std::enable_shared_from_this<BaseTxBuilder>
    {
    public:
        BaseTxBuilder(BaseTransaction& tx, SubTxID subTxID);
        virtual ~BaseTxBuilder() = default;

        BaseTransaction& m_Tx;
        SubTxID m_SubTxID;

        HeightRange m_Height;

        struct Coins
        {
            std::vector<Coin::ID> m_Input;
            std::vector<Coin::ID> m_Output;
            std::vector<IPrivateKeyKeeper2::ShieldedInput> m_InputShielded;

            bool IsEmpty() const {
                return m_Input.empty() && m_Output.empty() && m_InputShielded.empty();
            }

            void AddOffset(ECC::Scalar::Native&, Key::IKdf::Ptr& pMasterKdf) const;

        } m_Coins;

        Transaction::Ptr m_pTransaction;

        struct Balance
        {
            struct Entry
            {
                Amount m_In = 0;
                Amount m_Out = 0;

                bool IsEnoughNetTx(Amount) const;
            };

            typedef std::map<Asset::ID, Entry> Map;

            Map m_Map;
            Amount m_Fees = 0;

            void Add(const Coin::ID&, bool bOutp);
            void Add(const IPrivateKeyKeeper2::ShieldedInput&);

        } m_Balance;

        void RefreshBalance();

        void AddPreselectedCoins();
        Amount MakeInputs(Amount, Asset::ID); // make the balance (outs - ins) at least this amount. Returns actual
        Amount MakeInputsAndChange(Amount, Asset::ID); // same as above, auto creates a change if necessary
        void SaveCoins();

        void AddOutput(const Coin::ID&);
        void CreateAddNewOutput(Coin::ID&);

        void VerifyTx(); // throws exc if invalid

        void GenerateInOuts();

        enum struct Stage {
            None,
            Done,
            InProgress
        };

        Stage m_GeneratingInOuts = Stage::None;
        Stage m_Signing = Stage::None;

        bool IsGeneratingInOuts() const;
        bool IsSigning() const;

        Amount m_Fee = 0;

        void VerifyAssetsEnabled(); // throws exc if disabled

        template <typename T>
        bool GetParameter(TxParameterID paramID, T& value) const {
            return m_Tx.GetParameter(paramID, value, m_SubTxID);
        }

        template <typename T>
        void GetParameterStrict(TxParameterID paramID, T& value) const {
            if (!GetParameter(paramID, value))
                throw TransactionFailedException(true, TxFailureReason::FailedToGetParameter);
        }

        template <typename T>
        T GetParameterStrict(TxParameterID paramID) const {
            T value;
            GetParameterStrict(paramID, value);
            return std::move(value);
        }

        template <typename T>
        bool SetParameter(TxParameterID paramID, const T& value) {
            return m_Tx.SetParameter(paramID, value, m_SubTxID);
        }

        TxKernelStd* m_pKrn = nullptr;
        std::string GetKernelIDString() const;

        void CheckMinimumFee(const TxStats* pFromPeer = nullptr);

        virtual bool SignTx(); // returns if negotiation is complete and all specified in/outs are generated

        struct Status {
            typedef uint8_t Type;

            static const Type None = 0;
            static const Type FullTx = 100; //  transaction is fully built and validated
        };

        Status::Type m_Status = Status::None;

        void FinalyzeTx(); // normalize, verify, and set status
        // Call when all tx elements are added

    protected:

        virtual bool IsConventional() { return true; }
        virtual void FinalyzeTxInternal();

        void MakeInputs(Balance::Entry&, Amount, Asset::ID); // make the balance (outs - ins) at least this amount. Returns actual

        struct KeyKeeperHandler
            :public IPrivateKeyKeeper2::Handler
        {
            std::weak_ptr<BaseTxBuilder> m_pBuilder;
            Stage* m_pStage;

            KeyKeeperHandler(BaseTxBuilder&, Stage& s);
            ~KeyKeeperHandler();

            virtual void OnDone(IPrivateKeyKeeper2::Status::Type) override;

            virtual void OnSuccess(BaseTxBuilder&) = 0;
            virtual void OnFailed(BaseTxBuilder&, IPrivateKeyKeeper2::Status::Type);

            void Detach(BaseTxBuilder&, Stage);
            void OnAllDone(BaseTxBuilder&);
        };

        void TagInput(const CoinID&);

        void SetInOuts(IPrivateKeyKeeper2::Method::TxCommon&);
        void SetCommon(IPrivateKeyKeeper2::Method::TxCommon&);

        template<typename T1, typename T2>
        void SaveAndStore(T2& dest, TxParameterID parameterID, const T1& source)
        {
            dest = source;
            SetParameter(parameterID, dest);
        }

        struct HandlerInOuts;
        void SaveInOuts();

        void AddOffset(const ECC::Scalar&);
        void AddOffset(const ECC::Scalar::Native&);

        static bool Aggregate(ECC::Point&, const ECC::Point::Native&);
        static bool Aggregate(ECC::Point&, ECC::Point::Native&, const ECC::Point&);

        void AddKernel(TxKernelStd::Ptr&);
        void SaveKernel();
        void SaveKernelID();

        void SetStatus(Status::Type);
    };



    class SimpleTxBuilder
        :public BaseTxBuilder
    {
    public:
        SimpleTxBuilder(BaseTransaction& tx, SubTxID subTxID);
        virtual ~SimpleTxBuilder() = default;

        Amount m_Amount = 0;
        Asset::ID m_AssetID = 0;

        Height m_Lifetime;

        void MakeInputsAndChanges();

        struct Status
            :public BaseTxBuilder::Status
        {
            static const Type SelfSigned = 1; // kernel fully signed, in/outs ready
        };

        virtual bool SignTx() override;

    protected:
        void SignSplit();
    };


    class MutualTxBuilder
        :public SimpleTxBuilder
    {
    public:
        MutualTxBuilder(BaseTransaction& tx, SubTxID subTxID);
        virtual ~MutualTxBuilder() = default;

        bool m_IsSender = false;

        struct Status
            :public SimpleTxBuilder::Status
        {
            static const Type SndHalf = 1;
            static const Type SndHalfSent = 2;
            static const Type SndFullHalfSig = 3;
            static const Type SndFull = 4;

            static const Type RcvHalf = 1;
            static const Type RcvFullHalfSig = 2;
            static const Type RcvFullHalfSigSent = 3;

        };

        virtual bool SignTx() override;

    protected:
        void FinalyzeMaxHeight();
        void CreateKernel(TxKernelStd::Ptr&);
        void SignSender(bool initial);
        void SignReceiver();
        bool LoadPeerPart(ECC::Point::Native& ptNonce, ECC::Point::Native& ptExc);
        void AddPeerOffset();
        virtual bool SignTxSender();
        virtual bool SignTxReceiver();

        virtual void SendToPeer(SetTxParameter&&) = 0;
        virtual void FinalyzeTxInternal() override; // Adds peer's in/outs/offset (if provided), and calls base
        virtual void AddPeerSignature(const ECC::Point::Native& ptNonce, const ECC::Point::Native& ptExc);

    };

}

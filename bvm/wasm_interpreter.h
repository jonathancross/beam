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
#include "../utility/common.h"
#include "../utility/containers.h"

namespace beam {
namespace Wasm {

	void Fail();
	void Test(bool);

	typedef uint32_t Word;

	struct TypeCode
	{
		static const uint8_t i32 = 0x7F;
		static const uint8_t i64 = 0x7E;
		static const uint8_t f32 = 0x7D;
		static const uint8_t f64 = 0x7C;
	};

	class Reader
	{
		template <typename T, bool bSigned>
		T ReadInternal();
	public:

		const uint8_t* m_p0;
		const uint8_t* m_p1;

		void Ensure(uint32_t n);
		const uint8_t* Consume(uint32_t n);

		uint8_t Read1() { return *Consume(1); }

		template <typename T>
		T Read();

		template <typename T>
		void Read(T& x) {
			x = Read<T>();
		}
	};


	struct Compiler
	{
		struct Context;

		template <typename T>
		struct Vec
		{
			uint32_t n;
			const T* p;

			void Read(Reader& inp) {
				inp.Read(n);
				ReadArr(inp);
			}

			void ReadArr(Reader& inp) {
				p = reinterpret_cast<const T*>(inp.Consume(sizeof(T) * n));
			}
		};

		struct PerType {
			Vec<uint8_t> m_Args;
			Vec<uint8_t> m_Rets;
		};

		std::vector<PerType> m_Types;

		struct PerImport {
			Vec<char> m_sMod;
			Vec<char> m_sName;
			uint32_t m_Binding; // set by the env before compiling
		};

		struct PerImportFunc :public PerImport {
			uint32_t m_TypeIdx;
		};
		std::vector<PerImportFunc> m_ImportFuncs;

		struct GlobalVar {
			uint8_t m_Type;
			uint8_t m_IsVariable;
		};

		struct PerImportGlobal
			:public PerImport
			,public GlobalVar
		{
		};
		std::vector<PerImportGlobal> m_ImportGlobals;

		struct PerFunction
		{
			uint32_t m_TypeIdx;

			struct Locals
			{
				struct PerType {
					uint8_t m_Type;
					uint8_t m_SizeWords;
					uint32_t m_PosWords;
				};

				std::vector<PerType> m_v; // including args

				uint32_t get_SizeWords() const;
				void Add(uint8_t nType);

			} m_Locals;

			Reader m_Expression;
		};

		std::vector<PerFunction> m_Functions;

		std::vector<GlobalVar> m_Globals;

		struct PerExport {
			Vec<char> m_sName;
			uint8_t m_Kind; // 0 for function
			uint32_t m_Idx; // Type index for function
		};

		std::vector<PerExport> m_Exports;

		struct LabelSet
		{
			struct Target
			{
				uint32_t m_Pos;
				uint32_t m_iItem;
			};

			std::vector<Target> m_Targets;
			std::vector<uint32_t> m_Items;
		};

		LabelSet m_Labels;
		ByteBuffer m_Data;
		ByteBuffer m_Result;
		Word m_Data0;

		void Parse(const Reader&); // parses the wasm file info, sets labels for local functions. No compilation yet.
		// Time to set the external bindings, create a header, etc.

		void Build();
	};

	struct MemoryType {
		static const Word Data   = 0;
		static const Word Global = 1U << (sizeof(Word) * 8 - 2);
		static const Word Stack  = 2U << (sizeof(Word) * 8 - 2);
		static const Word Mask   = 3U << (sizeof(Word) * 8 - 2);
	};

	enum struct VariableType
	{
		StackPointer = 0,
		count
	};


	struct Processor
	{
		Blob m_Code;
		Blob m_Data;
		Word m_Data0;
		Blob m_LinearMem;
		Reader m_Instruction;

		struct Stack
		{
			Word* m_pPtr;
			Word m_Pos = 0;
			Word m_BytesMax = 0; // total stack size
			Word m_BytesCurrent = 0; // operand stack remaining, before it collides with alias stack

			static const Word s_Alignment = 8;
			static Word AlignUp(Word);
			static void TestAlignmentPower(uint32_t);

			Word get_AlasSp() const;
			void set_AlasSp(Word);

			void AliasAlloc(Word nSize);
			void AliasFree(Word nSize);
			uint8_t* get_AliasPtr() const;


			void TestSelf() const;

			Word Pop1();
			void Push1(Word);

			uint64_t Pop2();
			void Push2(uint64_t);

			template <typename T>
			T Pop()
			{
				T res;
				static_assert(std::numeric_limits<T>::is_integer);
				if constexpr (sizeof(T) <= sizeof(Word))
				{
					res = static_cast<T>(Pop1());
					Log(static_cast<Word>(res), false);
				}
				else
				{
					static_assert(sizeof(T) <= sizeof(uint64_t));
					res = static_cast<T>(Pop2());
					Log(static_cast<uint64_t>(res), false);
				}

				return res;

			}

			template <typename T>
			void Push(const T& x)
			{
				static_assert(std::numeric_limits<T>::is_integer);

				if constexpr (sizeof(T) <= sizeof(Word))
				{
					Log(static_cast<Word>(x), true);
					Push1(static_cast<Word>(x));
				}
				else
				{
					static_assert(sizeof(T) <= sizeof(uint64_t));
					Log(static_cast<uint64_t>(x), true);
					Push2(x);
				}
			}

			IMPLEMENT_GET_PARENT_OBJ(Processor, m_Stack) // for logging

		private:
			template <typename T> void Log(T, bool bPush);

		} m_Stack;

		struct Dbg
		{
			std::ostringstream* m_pOut = nullptr;
			bool m_Stack = false;
			bool m_Instructions = false;
			bool m_ExtCall = false;
		} m_Dbg;


		Word get_Ip() const;
		void Jmp(uint32_t ip);

		void RunOnce();

		uint8_t* get_AddrEx(uint32_t nOffset, uint32_t nSize, bool bW);

		uint8_t* get_AddrW(uint32_t nOffset, uint32_t nSize) {
			return get_AddrEx(nOffset, nSize, true);
		}

		const uint8_t* get_AddrR(uint32_t nOffset, uint32_t nSize) {
			return get_AddrEx(nOffset, nSize, false);
		}

		virtual void OnCall(Word nAddr);
		virtual void OnRet(Word nRetAddr);

		virtual void InvokeExt(uint32_t);
		virtual void OnGlobalVar(uint32_t, bool bGet);

	};

} // namespace Wasm
} // namespace beam

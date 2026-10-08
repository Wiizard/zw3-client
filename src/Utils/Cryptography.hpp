#pragma once

namespace Utils::Cryptography
{
	void Initialize();

	class Token
	{
	public:
		Token() = default;
		Token(const std::string& token) : tokenString(token.begin(), token.end()) {}
		Token(const std::basic_string<std::uint8_t>& token) : tokenString(token) {}

		Token& operator++()
		{
			if (this->tokenString.empty())
			{
				this->tokenString.push_back(0);
				return *this;
			}

			for (auto i = static_cast<int>(this->tokenString.size()) - 1; i >= 0; --i)
			{
				if (this->tokenString[i] != 0xFF)
				{
					++this->tokenString[i];
					return *this;
				}

				this->tokenString[i] = 0;
			}

			this->tokenString.insert(this->tokenString.begin(), 0);
			return *this;
		}

		Token operator++(int)
		{
			const Token result = *this;
			this->operator++();
			return result;
		}

		bool operator==(const Token& token) const
		{
			return this->tokenString == token.tokenString;
		}

		bool operator<(const Token& token) const
		{
			if (this->tokenString.size() != token.tokenString.size())
			{
				return this->tokenString.size() < token.tokenString.size();
			}

			return this->tokenString < token.tokenString;
		}

		bool operator>(const Token& token) const
		{
			return token < *this;
		}

		bool operator<=(const Token& token) const
		{
			return !(*this > token);
		}

		bool operator>=(const Token& token) const
		{
			return !(*this < token);
		}

		[[nodiscard]] std::string ToString() const
		{
			return std::string{this->tokenString.begin(), this->tokenString.end()};
		}

		[[nodiscard]] std::basic_string<std::uint8_t> ToUnsignedString() const
		{
			return this->tokenString;
		}

		void Clear()
		{
			this->tokenString.clear();
		}

	private:
		std::basic_string<std::uint8_t> tokenString;
	};

	class Rand
	{
	public:
		static std::uint64_t GenerateLong();
		static std::uint32_t GenerateInt();
		static std::string GenerateChallenge();
		static void Initialize();

	private:
		static prng_state state;
	};

	class ECC
	{
	public:
		class Key
		{
		public:
			Key() : keyStorage(new ecc_key{}, Release) {}

			[[nodiscard]] bool IsValid() const
			{
				return !Memory::IsSet(this->keyStorage.get(), 0, sizeof(ecc_key));
			}

			[[nodiscard]] ecc_key* GetKeyPtr() const
			{
				return this->keyStorage.get();
			}

			[[nodiscard]] std::string GetPublicKey() const;
			[[nodiscard]] std::string Serialize(int type = PK_PRIVATE) const;

			void Set(const std::string& publicKeyBuffer);
			void Deserialize(const std::string& key);
			void Free() const;

			bool operator==(const Key& key) const
			{
				return this->IsValid() && key.IsValid() && this->Serialize(PK_PUBLIC) == key.Serialize(PK_PUBLIC);
			}

		private:
			static void Release(ecc_key* key);

			std::shared_ptr<ecc_key> keyStorage;
		};

		static Key GenerateKey(int bits, const std::string& entropy = {});
		static std::string SignMessage(const Key& key, const std::string& message);
		static bool VerifyMessage(const Key& key, const std::string& message, const std::string& signature);
	};

	class RSA
	{
	public:
		class Key
		{
		public:
			Key() : keyStorage(new rsa_key{}, Release) {}

			[[nodiscard]] bool IsValid() const
			{
				return !Memory::IsSet(this->keyStorage.get(), 0, sizeof(rsa_key));
			}

			[[nodiscard]] rsa_key* GetKeyPtr() const
			{
				return this->keyStorage.get();
			}

			[[nodiscard]] std::string GetPublicKey() const;
			[[nodiscard]] std::string Serialize(int type = PK_PRIVATE) const;

			void Set(const std::string& keyBuffer);
			void Free() const;

		private:
			static void Release(rsa_key* key);

			std::shared_ptr<rsa_key> keyStorage;
		};

		static Key GenerateKey(int bits);
		static std::string SignMessage(const Key& key, const std::string& message);
		static bool VerifyMessage(const Key& key, const std::string& message, const std::string& signature);
	};

	class SHA1
	{
	public:
		static std::string Compute(const std::string& data, bool hex = false);
	};

	class SHA256
	{
	public:
		static std::string Compute(const std::string& data, bool hex = false);
	};

	class SHA512
	{
	public:
		static std::string Compute(const std::string& data, bool hex = false);
	};

	class JenkinsOneAtATime
	{
	public:
		static std::uint32_t Compute(const std::string& data);
		static std::uint32_t Compute(const char* key, std::size_t len);
	};
}

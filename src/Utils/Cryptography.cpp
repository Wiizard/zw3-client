#include "Cryptography.hpp"

namespace Utils::Cryptography
{
	prng_state Rand::state;

	void Initialize()
	{
		Rand::Initialize();
	}

	void Rand::Initialize()
	{
		ltc_mp = ltm_desc;
		register_prng(&fortuna_desc);
		rng_make_prng(128, find_prng("fortuna"), &Rand::state, nullptr);
	}

	std::uint64_t Rand::GenerateLong()
	{
		std::uint64_t number = 0;
		fortuna_read(reinterpret_cast<std::uint8_t*>(&number), sizeof(number), &Rand::state);
		return number;
	}

	std::uint32_t Rand::GenerateInt()
	{
		std::uint32_t number = 0;
		fortuna_read(reinterpret_cast<std::uint8_t*>(&number), sizeof(number), &Rand::state);
		return number;
	}

	std::string Rand::GenerateChallenge()
	{
		char buffer[512]{};
		int pos = 0;

		pos += sprintf_s(&buffer[pos], sizeof(buffer) - pos, "%X", GenerateInt());
		pos += sprintf_s(&buffer[pos], sizeof(buffer) - pos, "%X", ~timeGetTime() ^ GenerateInt());
		pos += sprintf_s(&buffer[pos], sizeof(buffer) - pos, "%X", GenerateInt());

		return std::string{ buffer, static_cast<std::size_t>(pos) };
	}

	void ECC::Key::Release(ecc_key* key)
	{
		if (key)
		{
			if (!Memory::IsSet(key, 0, sizeof(ecc_key)))
			{
				ecc_free(key);
			}

			delete key;
		}
	}

	void ECC::Key::Free() const
	{
		if (this->IsValid())
		{
			ecc_free(this->GetKeyPtr());
		}

		ZeroMemory(this->GetKeyPtr(), sizeof(ecc_key));
	}

	std::string ECC::Key::GetPublicKey() const
	{
		std::uint8_t buffer[512]{};
		unsigned long length = sizeof(buffer);

		if (ecc_ansi_x963_export(this->GetKeyPtr(), buffer, &length) != CRYPT_OK)
		{
			return {};
		}

		return std::string{reinterpret_cast<char*>(buffer), length};
	}

	std::string ECC::Key::Serialize(int type) const
	{
		std::uint8_t buffer[4096]{};
		unsigned long length = sizeof(buffer);

		if (ecc_export(buffer, &length, type, this->GetKeyPtr()) != CRYPT_OK)
		{
			return {};
		}

		return std::string{reinterpret_cast<char*>(buffer), length};
	}

	void ECC::Key::Set(const std::string& publicKeyBuffer)
	{
		this->Free();

		ltc_mp = ltm_desc;

		const auto* data = reinterpret_cast<const std::uint8_t*>(publicKeyBuffer.data());

		if (ecc_ansi_x963_import(data, static_cast<unsigned long>(publicKeyBuffer.size()), this->GetKeyPtr()) != CRYPT_OK)
		{
			ZeroMemory(this->GetKeyPtr(), sizeof(ecc_key));
		}
	}

	void ECC::Key::Deserialize(const std::string& key)
	{
		this->Free();

		ltc_mp = ltm_desc;

		const auto* data = reinterpret_cast<const std::uint8_t*>(key.data());

		if (ecc_import(data, static_cast<unsigned long>(key.size()), this->GetKeyPtr()) != CRYPT_OK)
		{
			ZeroMemory(this->GetKeyPtr(), sizeof(ecc_key));
		}
	}

	ECC::Key ECC::GenerateKey(int bits, const std::string& entropy)
	{
		Key key;

		ltc_mp = ltm_desc;

		if (entropy.empty())
		{
			register_prng(&sprng_desc);
			ecc_make_key(nullptr, find_prng("sprng"), bits / 8, key.GetKeyPtr());
			return key;
		}

		const auto descriptorIndex = register_prng(&chacha20_prng_desc);
		const auto state = std::make_unique<prng_state>();

		chacha20_prng_start(state.get());
		chacha20_prng_add_entropy(reinterpret_cast<const std::uint8_t*>(entropy.data()), static_cast<unsigned long>(entropy.size()), state.get());
		chacha20_prng_ready(state.get());

		ecc_make_key(state.get(), descriptorIndex, bits / 8, key.GetKeyPtr());

		return key;
	}

	std::string ECC::SignMessage(const Key& key, const std::string& message)
	{
		if (!key.IsValid())
		{
			return {};
		}

		std::uint8_t buffer[512]{};
		unsigned long length = sizeof(buffer);

		ltc_mp = ltm_desc;
		register_prng(&sprng_desc);

		const auto* data = reinterpret_cast<const std::uint8_t*>(message.data());

		ltc_ecc_sig_opts options{};
		options.type = LTC_ECCSIG_ANSIX962;
		options.wprng = find_prng("sprng");

		if (ecc_sign_hash_v2(data, static_cast<unsigned long>(message.size()), buffer, &length, &options, key.GetKeyPtr()) != CRYPT_OK)
		{
			return {};
		}

		return std::string{reinterpret_cast<char*>(buffer), length};
	}

	bool ECC::VerifyMessage(const Key& key, const std::string& message, const std::string& signature)
	{
		if (!key.IsValid())
		{
			return false;
		}

		ltc_mp = ltm_desc;

		const auto* signatureData = reinterpret_cast<const std::uint8_t*>(signature.data());
		const auto* messageData = reinterpret_cast<const std::uint8_t*>(message.data());

		ltc_ecc_sig_opts options{};
		options.type = LTC_ECCSIG_ANSIX962;

		auto status = 0;
		const auto result = ecc_verify_hash_v2(signatureData, static_cast<unsigned long>(signature.size()),
			messageData, static_cast<unsigned long>(message.size()), &options, &status, key.GetKeyPtr());

		return result == CRYPT_OK && status != 0;
	}

	void RSA::Key::Release(rsa_key* key)
	{
		if (key)
		{
			if (!Memory::IsSet(key, 0, sizeof(rsa_key)))
			{
				rsa_free(key);
			}

			delete key;
		}
	}

	void RSA::Key::Free() const
	{
		if (this->IsValid())
		{
			rsa_free(this->GetKeyPtr());
		}

		ZeroMemory(this->GetKeyPtr(), sizeof(rsa_key));
	}

	std::string RSA::Key::GetPublicKey() const
	{
		return this->Serialize(PK_PUBLIC);
	}

	std::string RSA::Key::Serialize(int type) const
	{
		std::uint8_t buffer[4096]{};
		unsigned long length = sizeof(buffer);

		if (rsa_export(buffer, &length, type, this->GetKeyPtr()) != CRYPT_OK)
		{
			return {};
		}

		return std::string{reinterpret_cast<char*>(buffer), length};
	}

	void RSA::Key::Set(const std::string& keyBuffer)
	{
		this->Free();

		ltc_mp = ltm_desc;

		const auto* data = reinterpret_cast<const std::uint8_t*>(keyBuffer.data());

		if (rsa_import(data, static_cast<unsigned long>(keyBuffer.size()), this->GetKeyPtr()) != CRYPT_OK)
		{
			ZeroMemory(this->GetKeyPtr(), sizeof(rsa_key));
		}
	}

	RSA::Key RSA::GenerateKey(int bits)
	{
		Key key;

		register_prng(&sprng_desc);

		ltc_mp = ltm_desc;

		rsa_make_key(nullptr, find_prng("sprng"), bits / 8, 65537, key.GetKeyPtr());

		return key;
	}

	std::string RSA::SignMessage(const Key& key, const std::string& message)
	{
		if (!key.IsValid())
		{
			return {};
		}

		std::uint8_t buffer[512]{};
		unsigned long length = sizeof(buffer);

		const auto hash = SHA512::Compute(message);
		const auto hashIndex = register_hash(&sha512_desc);

		ltc_mp = ltm_desc;

		ltc_rsa_op_parameters parameters{};
		parameters.prng = nullptr;
		parameters.wprng = 0;
		parameters.padding = LTC_PKCS_1_V1_5;
		parameters.params.saltlen = 0;
		parameters.params.hash_idx = hashIndex;
		parameters.params.mgf1_hash_idx = hashIndex;

		const auto* hashData = reinterpret_cast<const std::uint8_t*>(hash.data());

		if (rsa_sign_hash_v2(hashData, static_cast<unsigned long>(hash.size()), buffer, &length, &parameters, key.GetKeyPtr()) != CRYPT_OK)
		{
			return {};
		}

		return std::string{reinterpret_cast<char*>(buffer), length};
	}

	bool RSA::VerifyMessage(const Key& key, const std::string& message, const std::string& signature)
	{
		if (!key.IsValid())
		{
			return false;
		}

		const auto hash = SHA512::Compute(message);
		const auto hashIndex = register_hash(&sha512_desc);

		ltc_mp = ltm_desc;

		ltc_rsa_op_parameters parameters{};
		parameters.prng = nullptr;
		parameters.wprng = -1;
		parameters.padding = LTC_PKCS_1_V1_5;
		parameters.params.saltlen = 0;
		parameters.params.hash_idx = hashIndex;
		parameters.params.mgf1_hash_idx = hashIndex;

		const auto* signatureData = reinterpret_cast<const std::uint8_t*>(signature.data());
		const auto* hashData = reinterpret_cast<const std::uint8_t*>(hash.data());

		auto status = 0;
		const auto result = rsa_verify_hash_v2(signatureData, static_cast<unsigned long>(signature.size()),
			hashData, static_cast<unsigned long>(hash.size()), &parameters, &status, key.GetKeyPtr());

		return result == CRYPT_OK && status != 0;
	}

	std::string SHA1::Compute(const std::string& data, bool hex)
	{
		std::uint8_t buffer[20]{};

		hash_state state;
		sha1_init(&state);
		sha1_process(&state, reinterpret_cast<const std::uint8_t*>(data.data()), static_cast<unsigned long>(data.size()));
		sha1_done(&state, buffer);

		std::string hash{reinterpret_cast<char*>(buffer), sizeof(buffer)};

		if (!hex)
		{
			return hash;
		}

		return String::DumpHex(hash, {});
	}

	std::string SHA256::Compute(const std::string& data, bool hex)
	{
		std::uint8_t buffer[32]{};

		hash_state state;
		sha256_init(&state);
		sha256_process(&state, reinterpret_cast<const std::uint8_t*>(data.data()), static_cast<unsigned long>(data.size()));
		sha256_done(&state, buffer);

		std::string hash{reinterpret_cast<char*>(buffer), sizeof(buffer)};

		if (!hex)
		{
			return hash;
		}

		return String::DumpHex(hash, {});
	}

	std::string SHA512::Compute(const std::string& data, bool hex)
	{
		std::uint8_t buffer[64]{};

		hash_state state;
		sha512_init(&state);
		sha512_process(&state, reinterpret_cast<const std::uint8_t*>(data.data()), static_cast<unsigned long>(data.size()));
		sha512_done(&state, buffer);

		std::string hash{reinterpret_cast<char*>(buffer), sizeof(buffer)};

		if (!hex)
		{
			return hash;
		}

		return String::DumpHex(hash, {});
	}

	std::uint32_t JenkinsOneAtATime::Compute(const std::string& data)
	{
		return Compute(data.data(), data.size());
	}

	std::uint32_t JenkinsOneAtATime::Compute(const char* key, const std::size_t len)
	{
		std::uint32_t hash = 0;

		for (std::size_t i = 0; i < len; ++i)
		{
			hash += key[i];
			hash += (hash << 10);
			hash ^= (hash >> 6);
		}

		hash += (hash << 3);
		hash ^= (hash >> 11);
		hash += (hash << 15);
		return hash;
	}
}

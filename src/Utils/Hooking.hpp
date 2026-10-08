#pragma once

#define HOOK_JUMP true
#define HOOK_CALL false

namespace Utils
{
	class Hook
	{
	public:
		static bool Bind(std::uintptr_t idbImageBase);
		static bool IsBound();

		static std::uintptr_t Rebase(std::uintptr_t idbAddress);
		static std::uintptr_t Unrebase(std::uintptr_t liveAddress);

		class Signature
		{
		public:
			struct Container
			{
				const char* signature;
				const char* mask;
				std::function<void(char*)> callback;
			};

			Signature(void* start, std::size_t length) : start(start), length(length) {}
			Signature(std::uintptr_t start, std::size_t length)
				: Signature(reinterpret_cast<void*>(Rebase(start)), length) {}

			void Process();
			void Add(const Container& container);

		private:
			void* start;
			std::size_t length;
			std::vector<Container> signatures;
		};

		Hook() = default;

		Hook(void* site, void* replacement, bool asJump = true) { Initialize(site, replacement, asJump); }
		Hook(void* site, void(*replacement)(), bool asJump = true)
			: Hook(site, reinterpret_cast<void*>(replacement), asJump) {}

		Hook(std::uintptr_t site, void* replacement, bool asJump = true)
			: Hook(reinterpret_cast<void*>(Rebase(site)), replacement, asJump) {}
		Hook(std::uintptr_t site, void(*replacement)(), bool asJump = true)
			: Hook(site, reinterpret_cast<void*>(replacement), asJump) {}

		~Hook();

		Hook* Initialize(void* site, void* replacement, bool asJump = true);
		Hook* Initialize(std::uintptr_t site, void* replacement, bool asJump = true);
		Hook* Initialize(std::uintptr_t site, void(*replacement)(), bool asJump = true);

		Hook* Install(bool unprotect = true, bool keepUnprotected = false);
		Hook* Uninstall(bool unprotect = true);

		void* GetAddress();
		void* GetOriginal();
		bool IsInstalled();

		void Quick();

		template <typename T>
		static std::function<T> Call(std::uintptr_t function)
		{
			return std::function<T>(reinterpret_cast<T*>(Rebase(function)));
		}

		template <typename T>
		static std::function<T> Call(void* function)
		{
			return std::function<T>(reinterpret_cast<T*>(function));
		}

		static void SetString(void* place, const char* string, std::size_t length);
		static void SetString(std::uintptr_t place, const char* string, std::size_t length);
		static void SetString(void* place, const char* string);
		static void SetString(std::uintptr_t place, const char* string);

		static void Nop(void* place, std::size_t length);
		static void Nop(std::uintptr_t place, std::size_t length);

		static bool MatchesBytes(std::uintptr_t place, const std::uint8_t* expected, std::size_t length);

		static bool BranchesTo(std::uintptr_t site, std::uintptr_t target, bool asJump);

		struct LeaSite
		{
			std::uintptr_t address;
			std::array<std::uint8_t, 3> opcode;
			std::uintptr_t target;
		};

		static constexpr std::array<std::uint8_t, 3> leaRcx = { 0x48, 0x8D, 0x0D };
		static constexpr std::array<std::uint8_t, 3> leaRdx = { 0x48, 0x8D, 0x15 };
		static constexpr std::array<std::uint8_t, 3> leaR8 = { 0x4C, 0x8D, 0x05 };

		static bool IsLeaIntact(const LeaSite& lea);
		static bool CanLeaReach(const LeaSite& lea, const void* target);
		static void PointLeaAt(const LeaSite& lea, const void* target);

		static bool TryPointLeasAt(std::span<const LeaSite> leas, const char* text);
		static bool TryPointLeaAt(const LeaSite& lea, const char* text);

		static const char* PlaceNearImage(const char* text);

		static void RedirectJump(void* place, void* stub);
		static void RedirectJump(std::uintptr_t place, void* stub);

		static std::uintptr_t Trampoline(std::uintptr_t anchor, std::uintptr_t target);

		static void* AllocateDataNear(std::uintptr_t anchor, std::size_t size);

		template <typename T>
		static void Set(void* place, T value)
		{
			DWORD oldProtect;

			if (!VirtualProtect(place, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect))
			{
				return;
			}

			*static_cast<T*>(place) = value;

			VirtualProtect(place, sizeof(T), oldProtect, &oldProtect);
			FlushInstructionCache(GetCurrentProcess(), place, sizeof(T));
		}

		template <typename T>
		static void Set(std::uintptr_t place, T value)
		{
			return Set<T>(reinterpret_cast<void*>(Rebase(place)), value);
		}

		template <typename T>
		static void Xor(void* place, T value)
		{
			DWORD oldProtect;

			if (!VirtualProtect(place, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect))
			{
				return;
			}

			*static_cast<T*>(place) ^= value;

			VirtualProtect(place, sizeof(T), oldProtect, &oldProtect);
			FlushInstructionCache(GetCurrentProcess(), place, sizeof(T));
		}

		template <typename T>
		static void Xor(std::uintptr_t place, T value)
		{
			return Xor<T>(reinterpret_cast<void*>(Rebase(place)), value);
		}

		template <typename T>
		static void Or(void* place, T value)
		{
			DWORD oldProtect;

			if (!VirtualProtect(place, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect))
			{
				return;
			}

			*static_cast<T*>(place) |= value;

			VirtualProtect(place, sizeof(T), oldProtect, &oldProtect);
			FlushInstructionCache(GetCurrentProcess(), place, sizeof(T));
		}

		template <typename T>
		static void Or(std::uintptr_t place, T value)
		{
			return Or<T>(reinterpret_cast<void*>(Rebase(place)), value);
		}

		template <typename T>
		static void And(void* place, T value)
		{
			DWORD oldProtect;

			if (!VirtualProtect(place, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect))
			{
				return;
			}

			*static_cast<T*>(place) &= value;

			VirtualProtect(place, sizeof(T), oldProtect, &oldProtect);
			FlushInstructionCache(GetCurrentProcess(), place, sizeof(T));
		}

		template <typename T>
		static void And(std::uintptr_t place, T value)
		{
			return And<T>(reinterpret_cast<void*>(Rebase(place)), value);
		}

		template <typename T>
		static T Get(void* place)
		{
			return *static_cast<T*>(place);
		}

		template <typename T>
		static T Get(std::uintptr_t place)
		{
			return Get<T>(reinterpret_cast<void*>(Rebase(place)));
		}

	private:
		bool initialized = false;
		bool installed = false;

		void* place = nullptr;
		void* stub = nullptr;
		void* original = nullptr;
		char buffer[5]{};
		bool useJump = false;

		DWORD protection = 0;

		std::mutex stateMutex;
	};
}

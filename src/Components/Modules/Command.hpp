#pragma once

namespace Components
{
	class Command : public Component
	{
	public:
		class Params
		{
		public:
			Params() = default;
			virtual ~Params() = default;

			Params(Params&&) = delete;
			Params(const Params&) = delete;
			Params& operator=(Params&&) = delete;
			Params& operator=(const Params&) = delete;

			[[nodiscard]] virtual int Size() const = 0;
			[[nodiscard]] virtual const char* Get(int index) const = 0;
			[[nodiscard]] virtual std::string Join(int index) const;

			virtual const char* operator[](int index)
			{
				return this->Get(index);
			}
		};

		class ClientParams final : public Params
		{
		public:
			ClientParams();

			[[nodiscard]] int Size() const override;
			[[nodiscard]] const char* Get(int index) const override;

		private:
			int nesting;
		};

		class ServerParams final : public Params
		{
		public:
			ServerParams();

			[[nodiscard]] int Size() const override;
			[[nodiscard]] const char* Get(int index) const override;

		private:
			int nesting;
		};

		using Callback = std::function<void(const Params*)>;

		Command();

		static void Add(const char* name, const std::function<void()>& callback);
		static void Add(const char* name, const Callback& callback);
		static void AddRaw(const char* name, void(*callback)());
		static void AddSV(const char* name, const Callback& callback);

		static void Execute(std::string command, bool sync = true);

		static Game::cmd_function_s* Find(const std::string& command);

		static bool AddBindable(const char* name);

		static int GetBinding(const char* name);

		static void ExecBinding(int localClientNum, int binding, int key);

	private:
		static std::unordered_map<std::string, Callback> clientCallbacks;
		static std::unordered_map<std::string, Callback> serverCallbacks;

		static Game::cmd_function_s* Allocate();

		static void AddRawSV(const char* name, void(*callback)());

		static void MainCallback();
		static void MainCallbackSV();

		static bool TryExtendBindCommands();
		static void Key_ExecBinding_Hook(int localClientNum, int binding, int key);
	};
}

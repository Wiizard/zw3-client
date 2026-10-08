#pragma once

namespace Components
{
	class Component
	{
	public:
		Component() = default;
		virtual ~Component() = default;

#if defined(DEBUG)
		virtual std::string GetName()
		{
			std::string name = typeid(*this).name();
			const auto prefix = "class Components::"s;
			const auto at = name.find(prefix);

			if (at != std::string::npos)
			{
				name.erase(at, prefix.length());
			}

			return name;
		}
#endif
	};

	class Loader
	{
	public:
		static void Initialize();
		static void Register(Component* component);

		static bool IsPregame();

		template <typename T>
		static T* GetInstance()
		{
			for (auto& component : components)
			{
				if (typeid(*component) == typeid(T))
				{
					return reinterpret_cast<T*>(component);
				}
			}

			return nullptr;
		}

	private:
		static bool pregame;
		static std::vector<Component*> components;
	};
}

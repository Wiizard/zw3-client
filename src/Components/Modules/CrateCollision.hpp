#pragma once

namespace Components
{
	// adds a care_package brush to every map
	class CrateCollision : public Component
	{
	public:
		CrateCollision();

		static unsigned int GetIndex();

	private:
		static void CM_LoadMap_Hk(const char* name, unsigned int* checksum);
		static void* R_GetBrushModel_Hk(unsigned int index);
		static void Inject();
	};
}

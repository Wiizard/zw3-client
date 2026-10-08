#include "Components/Modules/BotAI/Navgen/Internal.hpp"
#include <cstdio>
#include <cstring>

namespace Components::BotAI::Navgen
{
	struct HurtVolume
	{
		float absMin[3];
		float absMax[3];
		int damage;
		const char* entity;
	};

	using SV_EntityContactBounds_t = int (__cdecl*)(const float* bounds, const void* entity);

	struct Objective
	{
		float position[3];
		char name[32];
		char gametype[16];
	};

	struct DynamicSolid
	{
		int entity;
		int contents;
	};

	static HurtVolume hurtVolumes[maxHurtVolumes];
	static int hurtVolumeCount = 0;
	static Objective objectives[maxObjectives];
	static int objectiveCount = 0;
	static DynamicSolid dynamicSolids[maxDynamicSolids];
	static int dynamicSolidCount = 0;
	static bool isSuppressed = false;


	static char* EntityAt(int entity)
	{
		return reinterpret_cast<char*>(g_entities) + gentityStride * entity;
	}

	const char* ClassnameText(unsigned short id)
	{
		if (!id)
		{
			return "";
		}
		const char* text = SL_ConvertToString(id);
		if (!text)
		{
			return "";
		}
		return text;
	}

	static const char* StringFieldText(const char* entity, int offset)
	{
		return ClassnameText(*reinterpret_cast<const unsigned short*>(entity + offset));
	}

	static bool StartsWith(const char* text, const char* prefix)
	{
		return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
	}

	static bool IsObjectiveName(const char* targetname)
	{
		static const char* const nameTable[] = {
			"flag_primary", "flag_secondary", "bombzone", "dd_bombzone", "sd_bomb",
			"sab_bomb", "sab_bomb_allies", "sab_bomb_axis",
			"ctf_flag_allies", "ctf_flag_axis", "hq_hardpoint", "gtnw_zone",
		};
		for (const char* name : nameTable)
		{
			if (std::strcmp(targetname, name) == 0)
			{
				return true;
			}
		}
		return false;
	}

	static bool IsHurtEntity(const char* classname, const char* targetname)
	{
		if (std::strcmp(classname, "trigger_hurt") == 0)
		{
			return true;
		}
		return std::strcmp(targetname, "minefield") == 0
			|| std::strcmp(targetname, "minefield_click") == 0
			|| std::strcmp(targetname, "radiation") == 0;
	}

	static bool IsDynamicSolid(const char* entity, const char* classname, const char* targetname)
	{
		const int contents = *reinterpret_cast<const int*>(entity + gentityContents);
		if ((contents & maskPlayerSolidNoBodies) == 0)
		{
			return false;
		}
		if (*reinterpret_cast<const void* const*>(entity + gentityClient))
		{
			return false;
		}
		const unsigned char modelType = static_cast<unsigned char>(entity[gentityModelType]);
		if (modelType == gentityModelTypeTrigger || std::strcmp(classname, "worldspawn") == 0)
		{
			return false;
		}
		return std::strcmp(targetname, "care_package") == 0
			|| StartsWith(classname, "misc_turret")
			|| std::strcmp(classname, "script_model") == 0;
	}


	using CM_EntityString_t = const char* (__cdecl*)();

	struct NamedPoint
	{
		char name[32];
		float origin[3];
	};
	static constexpr int maxNamedPoints = 512;
	static NamedPoint namedPoints[maxNamedPoints];
	static int namedPointCount = 0;

	static const char* ReadQuoted(const char* cursor, char* out, int outSize)
	{
		while (*cursor && *cursor != '"' && *cursor != '}' && *cursor != '{')
		{
			++cursor;
		}
		if (*cursor != '"')
		{
			return nullptr;
		}
		++cursor;
		int used = 0;
		while (*cursor && *cursor != '"')
		{
			if (used < outSize - 1)
			{
				out[used++] = *cursor;
			}
			++cursor;
		}
		out[used] = 0;
		if (*cursor == '"')
		{
			++cursor;
		}
		return cursor;
	}

	struct EntityBlock
	{
		char classname[32];
		char targetname[32];
		char target[32];
		char gameobject[32];
		float origin[3];
		bool hasOrigin;
	};

	static bool ReadEntityBlock(const char** cursorInOut, EntityBlock* out)
	{
		const char* cursor = *cursorInOut;
		while (*cursor && *cursor != '{')
		{
			++cursor;
		}
		if (*cursor != '{')
		{
			return false;
		}
		++cursor;
		*out = {};
		char key[32];
		char value[96];
		while (*cursor && *cursor != '}')
		{
			const char* afterKey = ReadQuoted(cursor, key, sizeof(key));
			if (!afterKey)
			{
				break;
			}
			const char* afterValue = ReadQuoted(afterKey, value, sizeof(value));
			if (!afterValue)
			{
				break;
			}
			cursor = afterValue;
			if (std::strcmp(key, "classname") == 0)
			{
				std::strncpy(out->classname, value, sizeof(out->classname) - 1);
			}
			else if (std::strcmp(key, "targetname") == 0)
			{
				std::strncpy(out->targetname, value, sizeof(out->targetname) - 1);
			}
			else if (std::strcmp(key, "target") == 0)
			{
				std::strncpy(out->target, value, sizeof(out->target) - 1);
			}
			else if (std::strcmp(key, "script_gameobjectname") == 0)
			{
				std::strncpy(out->gameobject, value, sizeof(out->gameobject) - 1);
				if (char* space = std::strchr(out->gameobject, ' '))
				{
					*space = 0;
				}
			}
			else if (std::strcmp(key, "origin") == 0)
			{
				out->hasOrigin = std::sscanf(value, "%f %f %f", &out->origin[0], &out->origin[1], &out->origin[2]) == 3;
			}
		}
		while (*cursor && *cursor != '}')
		{
			++cursor;
		}
		if (*cursor == '}')
		{
			++cursor;
		}
		*cursorInOut = cursor;
		return true;
	}

	static const NamedPoint* FindNamedPoint(const char* name)
	{
		for (int i = 0; i < namedPointCount; ++i)
		{
			if (std::strcmp(namedPoints[i].name, name) == 0)
			{
				return &namedPoints[i];
			}
		}
		return nullptr;
	}

	static int FindObjectiveNear(const char* name, const float* position)
	{
		for (int i = 0; i < objectiveCount; ++i)
		{
			const float dx = objectives[i].position[0] - position[0];
			const float dy = objectives[i].position[1] - position[1];
			if (std::strcmp(objectives[i].name, name) == 0 && dx * dx + dy * dy < 64.0f * 64.0f)
			{
				return i;
			}
		}
		return -1;
	}

	static void ScanEntityString()
	{
		const char* entities = CM_EntityString();
		if (!entities)
		{
			Report("navgen: no entity string for this map, objectives of other gametypes unknown\n");
			return;
		}

		namedPointCount = 0;
		const char* cursor = entities;
		EntityBlock block;
		while (ReadEntityBlock(&cursor, &block))
		{
			if (!block.hasOrigin || !*block.targetname || namedPointCount >= maxNamedPoints)
			{
				continue;
			}
			NamedPoint& point = namedPoints[namedPointCount++];
			std::strncpy(point.name, block.targetname, sizeof(point.name) - 1);
			point.name[sizeof(point.name) - 1] = 0;
			point.origin[0] = block.origin[0];
			point.origin[1] = block.origin[1];
			point.origin[2] = block.origin[2];
		}

		int added = 0;
		cursor = entities;
		while (ReadEntityBlock(&cursor, &block))
		{
			if (!IsObjectiveName(block.targetname) || objectiveCount >= maxObjectives)
			{
				continue;
			}
			const float* position = nullptr;
			if (block.hasOrigin)
			{
				position = block.origin;
			}
			else if (*block.target)
			{
				const NamedPoint* visual = FindNamedPoint(block.target);
				if (visual)
				{
					position = visual->origin;
				}
			}
			if (!position)
			{
				Report("navgen: objective %s (%s) has no origin and no placed visual, skipped\n",
					   block.targetname, block.classname);
				continue;
			}
			const int live = FindObjectiveNear(block.targetname, position);
			if (live >= 0)
			{
				std::strncpy(objectives[live].gametype, block.gameobject, sizeof(objectives[live].gametype) - 1);
				objectives[live].gametype[sizeof(objectives[live].gametype) - 1] = 0;
				continue;
			}
			Objective& objective = objectives[objectiveCount++];
			objective.position[0] = position[0];
			objective.position[1] = position[1];
			objective.position[2] = position[2];
			std::strncpy(objective.name, block.targetname, sizeof(objective.name) - 1);
			objective.name[sizeof(objective.name) - 1] = 0;
			std::strncpy(objective.gametype, block.gameobject, sizeof(objective.gametype) - 1);
			objective.gametype[sizeof(objective.gametype) - 1] = 0;
			++added;
			Report("navgen: objective %s [%s] (%s) at %.0f %.0f %.0f, from the map's entity list\n",
				   objective.name, block.gameobject, block.classname,
				   objective.position[0], objective.position[1], objective.position[2]);
		}
		Report("navgen: %d objective(s) from the entity list, %d in all\n", added, objectiveCount);
	}

	void ScanWorldEntities()
	{
		SuppressDynamicSolids(false);
		hurtVolumeCount = 0;
		objectiveCount = 0;
		dynamicSolidCount = 0;

		for (int index = 0; index < gentityMaxEntities; ++index)
		{
			const char* entity = EntityAt(index);
			if (!entity[gentityIsLinked])
			{
				continue;
			}

			const char* classname = StringFieldText(entity, gentityClassname);
			const char* targetname = StringFieldText(entity, gentityTargetname);
			const float* absMid = reinterpret_cast<const float*>(entity + gentityAbsMid);
			const float* absHalf = reinterpret_cast<const float*>(entity + gentityAbsHalf);
			float absMin[3];
			float absMax[3];
			for (int axis = 0; axis < 3; ++axis)
			{
				absMin[axis] = absMid[axis] - absHalf[axis];
				absMax[axis] = absMid[axis] + absHalf[axis];
			}

			if (IsHurtEntity(classname, targetname) && hurtVolumeCount < maxHurtVolumes)
			{
				HurtVolume& volume = hurtVolumes[hurtVolumeCount++];
				for (int axis = 0; axis < 3; ++axis)
				{
					volume.absMin[axis] = absMin[axis];
					volume.absMax[axis] = absMax[axis];
				}
				volume.damage = *reinterpret_cast<const int*>(entity + gentityDamage);
				volume.entity = entity;
				Report("navgen: hurt volume %s (%s) %.0f %.0f %.0f .. %.0f %.0f %.0f, dmg %d\n",
					   classname, targetname, absMin[0], absMin[1], absMin[2],
					   absMax[0], absMax[1], absMax[2], volume.damage);
			}

			if (IsObjectiveName(targetname) && objectiveCount < maxObjectives)
			{
				Objective& objective = objectives[objectiveCount++];
				const unsigned char modelType = static_cast<unsigned char>(entity[gentityModelType]);
				if (modelType == gentityModelTypeTrigger || modelType == gentityModelTypeBrush)
				{
					objective.position[0] = (absMin[0] + absMax[0]) * 0.5f;
					objective.position[1] = (absMin[1] + absMax[1]) * 0.5f;
					objective.position[2] = absMax[2];
				}
				else
				{
					const float* origin = reinterpret_cast<const float*>(entity + gentityOrigin);
					objective.position[0] = origin[0];
					objective.position[1] = origin[1];
					objective.position[2] = origin[2];
				}
				std::strncpy(objective.name, targetname, sizeof(objective.name) - 1);
				objective.name[sizeof(objective.name) - 1] = 0;
				objective.gametype[0] = 0;
				Report("navgen: objective %s (%s) at %.0f %.0f %.0f\n", objective.name, classname,
					   objective.position[0], objective.position[1], objective.position[2]);
			}

			if (IsDynamicSolid(entity, classname, targetname) && dynamicSolidCount < maxDynamicSolids)
			{
				DynamicSolid& solid = dynamicSolids[dynamicSolidCount++];
				solid.entity = index;
				solid.contents = *reinterpret_cast<const int*>(entity + gentityContents);
			}
		}

		ScanEntityString();

		if (dynamicSolidCount)
		{
			Report("navgen: %d dynamic solid(s) present during generation\n", dynamicSolidCount);
			for (int i = 0; i < dynamicSolidCount; ++i)
			{
				const char* entity = EntityAt(dynamicSolids[i].entity);
				const float* origin = reinterpret_cast<const float*>(entity + gentityOrigin);
				Report("navgen:   %d: %s (%s) at %.0f %.0f %.0f\n", dynamicSolids[i].entity,
					   StringFieldText(entity, gentityClassname), StringFieldText(entity, gentityTargetname),
					   origin[0], origin[1], origin[2]);
			}
		}
	}


	int HurtVolumeCount()
	{
		return hurtVolumeCount;
	}

	bool IsInHurtVolume(const float* point)
	{
		const float bodyTop = point[2] + standHalfHeight * 2.0f;
		for (int i = 0; i < hurtVolumeCount; ++i)
		{
			const HurtVolume& volume = hurtVolumes[i];
			if (point[0] + boxHalfWidth < volume.absMin[0] || point[0] - boxHalfWidth > volume.absMax[0])
			{
				continue;
			}
			if (point[1] + boxHalfWidth < volume.absMin[1] || point[1] - boxHalfWidth > volume.absMax[1])
			{
				continue;
			}
			if (bodyTop < volume.absMin[2] || point[2] > volume.absMax[2])
			{
				continue;
			}

			const float body[6] = {
				point[0], point[1], point[2] + standHalfHeight,
				boxHalfWidth, boxHalfWidth, standHalfHeight,
			};
			if (SV_EntityContactBounds(body, volume.entity))
			{
				return true;
			}
		}
		return false;
	}


	int ObjectiveCount()
	{
		return objectiveCount;
	}

	const float* ObjectivePosition(int index)
	{
		return objectives[index].position;
	}

	const char* ObjectiveName(int index)
	{
		return objectives[index].name;
	}

	const char* ObjectiveGametype(int index)
	{
		return objectives[index].gametype;
	}


	int DynamicSolidCount()
	{
		return dynamicSolidCount;
	}

	void SuppressDynamicSolids(bool suppress)
	{
		if (suppress == isSuppressed)
		{
			return;
		}
		for (int i = 0; i < dynamicSolidCount; ++i)
		{
			int* contents = reinterpret_cast<int*>(EntityAt(dynamicSolids[i].entity) + gentityContents);
			if (suppress)
			{
				*contents = 0;
			}
			else
			{
				*contents = dynamicSolids[i].contents;
			}
		}
		isSuppressed = suppress;
	}
}

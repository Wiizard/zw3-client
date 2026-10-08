#pragma once

namespace Utils::Maths
{
	constexpr void VectorClear(float x[3])
	{
		x[0] = 0.0f;
		x[1] = 0.0f;
		x[2] = 0.0f;
	}

	constexpr void VectorNegate(float x[3])
	{
		x[0] = -x[0];
		x[1] = -x[1];
		x[2] = -x[2];
	}

	float DotProduct(const float v1[3], const float v2[3]);
	void VectorSubtract(const float va[3], const float vb[3], float out[3]);
	void VectorAdd(const float va[3], const float vb[3], float out[3]);
	void VectorCopy(const float in[3], float out[3]);
	void VectorScale(const float v[3], float scale, float out[3]);
	float Vec3SqrDistance(const float v1[3], const float v2[3]);
}

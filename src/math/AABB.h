#pragma once

#include <glm/glm.hpp>
#include "Ray.h"

using namespace glm;

struct AABB : public Hittable {
	AABB(void);
	AABB(const glm::vec3& Mi, const glm::vec3& Ma);

	void ExtendMax(const glm::vec3& Val);
	void ExtendMin(const glm::vec3& Val);

	void Extend(const glm::vec3& Pos);
	void Extend(const AABB& BBox);

	vec3 Center() const;

	float SurfaceArea(void) const;
	float SurfaceAreaHalf(void) const;

	void Reset();

	glm::vec3 min;
	glm::vec3 max;

	bool Intersect(const Ray& ray, HitInfo& hit) const;
	bool Intersect(const Ray& ray, HitInfo& hit, vec2& distances) const;

	bool operator==(const AABB& other);
	bool operator!=(const AABB& other);
};

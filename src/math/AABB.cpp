#include "AABB.h"
#include <stdint.h>

AABB::AABB(void) : min(FLT_MAX), max(-FLT_MAX) {}

void AABB::Reset() {
	AABB clear;
	*this = clear;
}

void AABB::ExtendMax(const glm::vec3& Val) {
	max = glm::max(max, Val);
}

void AABB::ExtendMin(const glm::vec3& Val) {
	min = glm::min(min, Val);
}

void AABB::Extend(const glm::vec3& Pos) {
	ExtendMax(Pos);
	ExtendMin(Pos);
}

float AABB::SurfaceArea(void) const {
	/*
	An AABB is really just a fancy way to say "rectangular prism" (although with axis alignment restrictions, unless if it was an OBB)
	First let's define side lengths of a rectangular prism

	Equation 1:
	X = Max_X - Min_X
	Y = Max_Y - Min_Y
	Z = Max_Z - Min_Z

	Equation 1 is true as long as Max is always greater on any axis than Min, and in our case that's always true
	However in a case where the bounding box uses its default values (see constructor) then everything is broken.
	This can be fixed by the good old assert. Also, with GLM's vector math, we can just do XYZ = Max - Min

	Next we need to compute the surface area of the AABB. As we all know, area is width * length.
	Surface area is the summation of the area of all polygons on an object (at least in an object that isn't
	"continous" like a perfect sphere defined by a radius and position, but rather something like a discretized 
	set of triangles). A rectangular prism contains 6 rectangles, and the surface area is the summation of all of
	them. However, we can optimize this.

	Now there's probably a some sort of theorem for this, but I will just call it the "rectanglular prism oppisite 
	side rectangle theorm" or OSR for sort. In OSR, suppose we look at the rectangular prisim from one of the axes. 
	We will see a rectangle with a surface area which we will call A. Now we flip our directions and view the rectangle
	we now see, it will also be having a surface area of A. We can combine these two into a pair when computing surface 
	area. Instead of computing and adding the result of A twice, we could compute A once and add 2A. Another thing to note
	is that for each pair we multiply it by 2 before adding it. We can factor 2 out to reduce 2 multiplications. See equation
	2 for an optimized verion of surface area calculations and their derivations.

	Equation 2:
	SA = XY + XY + XZ + XZ + YZ + YZ
	SA = 2XY + 2XZ + 2YZ
	SA = 2(XY + XZ + YX)

	And there we have it. Fast surface area calculations. 
	*/

	return SurfaceAreaHalf() * 2.0f;
}

// Taken from madman's blog. Seriously, that guy has some really good stuff on BVHs
float AABB::SurfaceAreaHalf(void) const {
	glm::vec3 SideLengths = max - min;

	return
		SideLengths.x * (SideLengths.y + SideLengths.z) +
		SideLengths.y *  SideLengths.z;
}

void AABB::Extend(const AABB& BBox) {
	ExtendMax(BBox.max);
	ExtendMin(BBox.min);
}

AABB::AABB(const glm::vec3& Mi, const glm::vec3& Ma) : min(Mi), max(Ma) {}

/*
	// Maybe if we stored each value in a vector before multiplying we could use vectorization


Area =
	SideLengths.x * SideLengths.y +
	SideLengths.x * SideLengths.z +
	SideLengths.y * SideLengths.z ;

		Area =
	SideLengths.x * SideLengths.y +
	SideLengths.y * SideLengths.z +
	SideLengths.z * SideLengths.x;


		glm::vec3 SideLengths = Max - Min;

	glm::vec3 Mult = glm::vec3(SideLengths.y, SideLengths.z, SideLengths.x);

	float Area = 2.0f * glm::dot(SideLengths, Mult);
*/

// AABB test by madmann
bool AABB::Intersect(const Ray& iray, HitInfo& hit, vec2& distances) const {
	vec3 t_node_min = min * iray.direction + iray.origin;
	vec3 t_node_max = max * iray.direction + iray.origin;

	vec3 t_min = glm::min(t_node_min, t_node_max);
	vec3 t_max = glm::max(t_node_min, t_node_max);

	float t_entry = glm::max(t_min.x, glm::max(t_min.y, t_min.z));
	float t_exit = glm::min(t_max.x, glm::min(t_max.y, glm::min(t_max.z, hit.depth)));
	distances =  vec2(t_entry, t_exit);
	return distances.x <= distances.y && distances.y > 0.0f;
}


bool AABB::Intersect(const Ray& iray, HitInfo& hit) const {
	vec2 junk;
	return Intersect(iray, hit, junk);
}

vec3 AABB::Center() const {
	return (min + max) * 0.5f;
}

bool AABB::operator==(const AABB& other) {
	return (
		min.x == other.min.x &&
		min.y == other.min.y &&
		min.z == other.min.z &&
		max.x == other.max.x &&
		max.y == other.max.y &&
		max.z == other.max.z
	);
}

bool AABB::operator!=(const AABB& other) {
	return !(*this == other);
}
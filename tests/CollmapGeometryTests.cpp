#include <cassert>
#include <iostream>
#include "Components/Modules/CollmapGeometry.hpp"

struct Geometry { unsigned count; const int* geoms; };
struct Model { Geometry* physCollmap; };

int main()
{
	using Components::CollmapGeometry::Available;
	int geom = 7;
	Geometry valid{1, &geom}, empty{0, &geom}, unloaded{1, nullptr};
	Model model{&valid};
	assert(Available(static_cast<const Model*>(nullptr)) == nullptr);
	assert(Available(&model) == &valid);
	// Reproduce a binding cleared between enumeration and injection.
	model.physCollmap = nullptr;
	assert(Available(&model) == nullptr);
	model.physCollmap = &empty;
	assert(Available(&model) == nullptr);
	model.physCollmap = &unloaded;
	assert(Available(&model) == nullptr);
	model.physCollmap = &valid;
	assert(Available(&model) == &valid);
	std::cout << "PASS: valid, cleared, empty and unloaded collision bindings\n";
}

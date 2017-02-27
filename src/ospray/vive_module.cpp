#include <iostream>

namespace ospvr {
	extern "C" void ospray_init_module_vive() {
		std::cout << "Loading OSPRay Vive module\n";
	}
}


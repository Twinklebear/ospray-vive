#include <iostream>
#include <ospray/common/OSPCommon.h>

namespace ospvr {
	extern "C" OSPRAY_DLLEXPORT void ospray_init_module_vive() {
		std::cout << "Loading OSPRay Vive module\n";
	}
}


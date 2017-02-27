#include <limits>
#include "vr_camera.h"
// We just use the Vr camera but tweak it
#include "vr_camera_ispc.h"

#ifdef _WIN32
#define _USE_MATH_DEFINES
#include <math.h> // M_PI
#endif

namespace ospvr {
	VrCamera::VrCamera() {
		ispcEquivalent = ispc::VrCamera_create(this);
	}

	std::string VrCamera::toString() const {
		return "ospvr::VrCamera";
	}

	void VrCamera::commit() {
		Camera::commit();

		// Get the params for the lowerleft and upperleft params we take
		lowerLeft = getParam2f("lowerLeft", vec2f(0.f, 0.f));
		upperRight = getParam2f("upperRight", vec2f(1.f, 1.f));

		dir = normalize(dir);
		vec3f dir_du = normalize(cross(dir, up));
		vec3f dir_dv = cross(dir_du, dir);

		vec3f org = pos;
		vec3f dir_00 = dir + lowerLeft.x*dir_du + lowerLeft.y*dir_dv;

		dir_du *= upperRight.x - lowerLeft.x;
		dir_dv *= upperRight.y - lowerLeft.y;

		ispc::VrCamera_set(getIE(), (const ispc::vec3f&)org,
				(const ispc::vec3f&)dir_00, (const ispc::vec3f&)dir_du,
				(const ispc::vec3f&)dir_dv);
	}

	OSP_REGISTER_CAMERA(VrCamera, vr);

}


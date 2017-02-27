#pragma once

#include "camera/Camera.h"

namespace ospvr {
	struct VrCamera : public Camera {
		VrCamera();
		virtual ~VrCamera() = default;

		virtual std::string toString() const override;
		virtual void commit() override;

		vec2f lowerLeft;
		vec2f upperRight;
	};

}


#include "vr_camera.ih"
#include "math/sampling.ih"

void VrCamera_initRay(uniform Camera *uniform _self, varying Ray &ray,
		const varying CameraSample &sample)
{
	uniform VrCamera *uniform self = (uniform VrCamera *uniform)_self;

	vec2f screen = sample.screen;
	vec3f org = self->org;

	screen = Camera_subRegion(_self, screen);

	vec3f dir = self->dir_00 + screen.x * self->dir_du + screen.y * self->dir_dv;

	setRay(ray, org, normalize(dir), self->super.nearClip, 1e20f);
}

export void *uniform VrCamera_create(void *uniform cppE) {
	uniform VrCamera *uniform self = uniform new uniform VrCamera;
	self->super.cppEquivalent = cppE;
	self->super.initRay = VrCamera_initRay;
	self->super.doesDOF = false;
	return self;
}

export void VrCamera_set(void *uniform _self,
		const uniform vec3f &org, const uniform vec3f &dir_00,
		const uniform vec3f &dir_du, const uniform vec3f &dir_dv)
{
	uniform VrCamera *uniform self = (uniform VrCamera *uniform)_self;
	self->org = org;
	self->dir_00 = dir_00;
	self->dir_du = dir_du;
	self->dir_dv = dir_dv;
	self->super.doesDOF = false;
}

#include <iostream>
#include <array>
#include <iomanip>
#include <vector>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <ospray/ospray.h>
#include <ospcommon/vec.h>
#include <ospcommon/AffineSpace.h>
#include <openvr.h>
#include "gl_core_3_3.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

static int WIN_WIDTH = 1280/2;
static int WIN_HEIGHT = 720/2;

// We only have final resolve textures for the eyes
// since we don't need MSAA render targets on the GPU like
// in the samples
struct EyeResolveFB {
	GLuint resolve_fb;
	GLuint resolve_texture;
};

ospcommon::AffineSpace3f convert_vr_mat(const vr::HmdMatrix34_t &m) {
	using namespace ospcommon;
	return AffineSpace3f(
			vec3f(m.m[0][0], m.m[1][0], m.m[2][0]),
			vec3f(m.m[0][1], m.m[1][1], m.m[2][1]),
			vec3f(m.m[0][2], m.m[1][2], m.m[2][2]),
			vec3f(m.m[0][3], m.m[1][3], m.m[2][3]));
}

int main(int argc, const char **argv) {
	if (argc < 2) {
		std::cerr << "You must specify a model to render\n";
		return 1;
	}
	const std::string model_file = argv[1];
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
		std::cerr << "Failed to initialize SDL\n";
		return 1;
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#ifndef NDEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

	SDL_Window *win = SDL_CreateWindow("OSPRay + Vive", SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_OPENGL);
	if (!win){
		std::cout << "Failed to open SDL window: " << SDL_GetError() << "\n";
		return 1;
	}
	SDL_GLContext ctx = SDL_GL_CreateContext(win);
	if (!ctx){
		std::cout << "Failed to get OpenGL context: " << SDL_GetError() << "\n";
		return 1;
	}
	if (ogl_LoadFunctions() == ogl_LOAD_FAILED){
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to Load OpenGL Functions",
				"Could not load OpenGL functions for 3.3, OpenGL 3.3 or higher is required",
				NULL);
		SDL_GL_DeleteContext(ctx);
		SDL_DestroyWindow(win);
		SDL_Quit();
		return 1;
	}
	SDL_GL_SetSwapInterval(0);

	ospInit(&argc, argv);
	// Load our custom Vive code for OSPRay
	if (ospLoadModule("vive") != OSP_NO_ERROR) {
    std::cout << "Error loading vive module for OSPRay\n";
    return 1;
  }

	// Setup OpenVR system
	vr::EVRInitError vr_error;
	vr::IVRSystem *vr_system = vr::VR_Init(&vr_error, vr::VRApplication_Scene);
	if (vr_error != vr::VRInitError_None) {
		std::cout << "OpenVR Init error " << vr_error << "\n";
		return 1;
	}
	if (!vr_system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd)) {
		std::cout << "OpenVR HMD not tracking! Check connection and restart\n";
		return 1;
	}
	if (!vr::VRCompositor()) {
		std::cout << "OpenVR Failed to initialize compositor\n";
		return 1;
	}
	std::array<uint32_t, 2> vr_render_dims;
	vr_system->GetRecommendedRenderTargetSize(&vr_render_dims[0], &vr_render_dims[1]);
	std::cout << "OpenVR recommended render target resolution = " << vr_render_dims[0]
		<< "x" << vr_render_dims[1] << "\n";
	// use Vive's screen resolution
	vr_render_dims[0] = 1080;
	vr_render_dims[1] = 1200;
	std::cout << "App render target resolution = " << vr_render_dims[0]
		<< "x" << vr_render_dims[1] << "\n";

	GLuint fbo, texture;
	glGenFramebuffers(1, &fbo);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, vr_render_dims[0] * 2, vr_render_dims[1], 0, GL_RGBA,
			GL_UNSIGNED_BYTE, nullptr);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Setup resolve targets for the eyes
	std::array<EyeResolveFB, 2> eye_targets;
	for (size_t i = 0; i < eye_targets.size(); ++i) {
		glGenFramebuffers(1, &eye_targets[i].resolve_fb);
		glGenTextures(1, &eye_targets[i].resolve_texture);
		glBindTexture(GL_TEXTURE_2D, eye_targets[i].resolve_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, vr_render_dims[0], vr_render_dims[1], 0, GL_RGBA,
				GL_UNSIGNED_BYTE, nullptr);

		glBindFramebuffer(GL_FRAMEBUFFER, eye_targets[i].resolve_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
				eye_targets[i].resolve_texture, 0);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	using namespace ospcommon;
	// We render both left/right eye to the same framebuffer so we need it to be
	// 2x the width
	const vec2i image_size(vr_render_dims[0], vr_render_dims[1]);

	// TODO BUG: OSPRay's side-by-side camera can't do proper stereo because it
	// uses the same imageStart and imageEnd for both eyes
	std::array<OSPCamera, 2> cameras;
	// OSPRay does the interpupillary offset, but we do it ourselves directly
	std::array<vec3f, 2> eye_offsets;
	const std::array<vec3f, 2> eye_dirs = { vec3f(0.0f, 0.0f, -1.0f), vec3f(0.0f, 0.0f, -1.0f) };
	for (size_t i = 0; i < cameras.size(); ++i) {
		std::cout << "Eye = " << (i == 0 ? " left" : " right") << "\n";
		auto eye_mat = vr_system->GetEyeToHeadTransform(i == 0 ? vr::Eye_Left : vr::Eye_Right);
		eye_offsets[i] = vec3f(eye_mat.m[0][3], eye_mat.m[1][3], eye_mat.m[2][3]);

		float left, right, top, bottom;
		vr_system->GetProjectionRaw(i == 0 ? vr::Eye_Left : vr::Eye_Right, &left, &right, &top, &bottom);

		cameras[i] = ospNewCamera("vr");

		// move image plane (it is shifted to a side)
		// OpenVR has +y axis pointing down so we flip bottom and top
		ospSet2f(cameras[i], "lowerLeft", left, top);
		ospSet2f(cameras[i], "upperRight", right, bottom);
	}

	// Load the model w/ tinyobjloader
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, model_file.c_str(),
			nullptr, true);
	if (!err.empty()) {
		std::cerr << "Error loading model: " << err << "\n";
	}
	if (!ret) {
		return 1;
	}

	OSPModel world = ospNewModel();
	// Load all the objects into ospray
	OSPData pos_data = ospNewData(attrib.vertices.size() / 3, OSP_FLOAT3,
			attrib.vertices.data(), OSP_DATA_SHARED_BUFFER);
	ospCommit(pos_data);
	for (size_t s = 0; s < shapes.size(); ++s) {
		std::cout << "Loading mesh " << shapes[s].name
			<< ", has " << shapes[s].mesh.indices.size() << " vertices\n";
		const tinyobj::mesh_t &mesh = shapes[s].mesh;
		std::vector<int32_t> indices;
		indices.reserve(mesh.indices.size());
		for (const auto &idx : mesh.indices) {
			indices.push_back(idx.vertex_index);
		}
		OSPData idx_data = ospNewData(indices.size() / 3, OSP_INT3, indices.data());
		ospCommit(idx_data);
		OSPGeometry geom = ospNewGeometry("triangles");
		ospSetObject(geom, "vertex", pos_data);
		ospSetObject(geom, "index", idx_data);
		ospCommit(geom);
		ospAddGeometry(world, geom);
	}
	ospCommit(world);

	OSPRenderer renderer = ospNewRenderer("raycast_Ns");
	ospSetObject(renderer, "model", world);
	ospSetObject(renderer, "camera", cameras[0]);
	ospSetVec3f(renderer, "bgColor", (osp::vec3f&)vec3f(0.05));
	ospCommit(renderer);

	std::array<OSPFrameBuffer, 2> framebuffers;
	for (size_t i = 0; i < framebuffers.size(); ++i) {
		framebuffers[i] = ospNewFrameBuffer((osp::vec2i&)image_size, OSP_FB_SRGBA, OSP_FB_COLOR);
		ospFrameBufferClear(framebuffers[i], OSP_FB_COLOR);
	}

	std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> tracked_device_poses;
	bool quit = false;
	uint32_t prev_time = SDL_GetTicks();
	const std::string win_title = "OSPRay + Vive - OSPRay time for both eyes ";
	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e)){
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)){
				quit = true;
				break;
			}
		}

		// Update camera based on HMD position
		vr::VRCompositor()->WaitGetPoses(tracked_device_poses.data(), tracked_device_poses.size(), NULL, 0);
		const AffineSpace3f hmd_mat
			= convert_vr_mat(tracked_device_poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
		//std::cout << "hmd_mat = [\n" << hmd_mat << "]\n";


		// Render each eye and upload them
		uint32_t elapsed = 0;
		for (size_t i = 0; i < framebuffers.size(); ++i) {
			const uint32_t prev_time = SDL_GetTicks();

			// Transform the eye based on the head position
			const vec3f eye_pos = xfmPoint(hmd_mat, eye_offsets[i]);
			const vec3f eye_dir = xfmVector(hmd_mat, eye_dirs[i]);
			const vec3f cam_up = xfmVector(hmd_mat, vec3f(0, 1, 0));
			ospSetVec3f(cameras[i], "pos", (osp::vec3f&)eye_pos);
			ospSetVec3f(cameras[i], "dir", (osp::vec3f&)eye_dir);
			ospSetVec3f(cameras[i], "up",  (osp::vec3f&)cam_up);
			ospCommit(cameras[i]);
			ospSetObject(renderer, "camera", cameras[i]);
			ospCommit(renderer);

			ospFrameBufferClear(framebuffers[i], OSP_FB_COLOR);
			ospRenderFrame(framebuffers[i], renderer, OSP_FB_COLOR);
			const uint32_t cur_time = SDL_GetTicks();
			elapsed += cur_time - prev_time;

			const uint32_t *fb = static_cast<const uint32_t*>(ospMapFrameBuffer(framebuffers[i], OSP_FB_COLOR));
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, vr_render_dims[0] * i, 0, vr_render_dims[0], vr_render_dims[1],
					GL_RGBA, GL_UNSIGNED_BYTE, fb);
			ospUnmapFrameBuffer(fb, framebuffers[i]);
		}
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);

		// Blit the left/right eye halves of the ospray framebuffer to the left/right resolve targets
		// and submit them
		for (size_t i = 0; i < eye_targets.size(); ++i) {
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, eye_targets[i].resolve_fb);
			glBlitFramebuffer(vr_render_dims[0] * i, 0, vr_render_dims[0] * i + vr_render_dims[0], vr_render_dims[1],
					0, 0, vr_render_dims[0], vr_render_dims[1], GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
		vr::Texture_t left_eye = {};
		left_eye.handle = reinterpret_cast<void*>(eye_targets[0].resolve_texture);
		left_eye.eType = vr::TextureType_OpenGL;
		left_eye.eColorSpace = vr::ColorSpace_Gamma;

		vr::Texture_t right_eye = {};
		right_eye.handle = reinterpret_cast<void*>(eye_targets[1].resolve_texture);
		right_eye.eType = vr::TextureType_OpenGL;
		right_eye.eColorSpace = vr::ColorSpace_Gamma;

		vr::VRCompositor()->Submit(vr::Eye_Left, &left_eye, nullptr);//, vr::Submit_LensDistortionAlreadyApplied);
		vr::VRCompositor()->Submit(vr::Eye_Right, &right_eye, nullptr);//, vr::Submit_LensDistortionAlreadyApplied);
		glFlush();

		// Blit the app window display
#if 1
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, vr_render_dims[0] * 2, vr_render_dims[1], 0, 0, WIN_WIDTH, WIN_HEIGHT,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);
#endif
		SDL_GL_SwapWindow(win);

#if 1
		const std::string title = win_title + std::to_string(elapsed) + "ms";
		SDL_SetWindowTitle(win, title.c_str());
#endif
	}

	vr::VR_Shutdown();
	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}


#include <iostream>
#include <array>
#include <vector>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <ospray.h>
#include <ospcommon/vec.h>
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

	ospInit(&argc, argv);

	using namespace ospcommon;
	// We render both left/right eye to the same framebuffer so we need it to be
	// 2x the width
	const vec2i image_size(vr_render_dims[0], vr_render_dims[1]);
	const vec3f cam_pos(0, 50, 280);
	const vec3f cam_target = vec3f(0, 40, 0);
	const vec3f cam_up(0, 1, 0);

	// TODO BUG: OSPRay's side-by-side camera can't do proper stereo because it
	// uses the same look direction for both eyes
	std::array<OSPCamera, 2> cameras;
	for (size_t i = 0; i < cameras.size(); ++i) {
		auto eye_mat = vr_system->GetEyeToHeadTransform(i == 0 ? vr::Eye_Left : vr::Eye_Right);
		std::cout << "[\n";
		for (size_t r = 0; r < 3; ++r) {
			for (size_t c = 0; c < 4; ++c) {
				std::cout << eye_mat.m[r][c] << "  ";
			}
			std::cout << "\n";
		}
		std::cout << "]\n";
		cameras[i] = ospNewCamera("perspective");
		ospSetf(cameras[i], "aspect", image_size.x / static_cast<float>(image_size.y));
		// TODO: How to query the HMD IPD?
		//ospSet1i(cameras[i], "stereoMode", 0);
		//ospSet1f(camera, "interpupillaryDistance", 0.0635);
		vec3f eye_pos = cam_pos;
		if (i == 0) {
			eye_pos = eye_pos - vec3f(0.0318, 0, 0);
		} else {
			eye_pos = eye_pos + vec3f(0.0318, 0, 0);
		}
		vec3f eye_dir = cam_target - eye_pos;
		ospSetVec3f(cameras[i], "pos", (osp::vec3f&)eye_pos);
		ospSetVec3f(cameras[i], "dir", (osp::vec3f&)eye_dir);
		ospSetVec3f(cameras[i], "up",  (osp::vec3f&)cam_up);
		ospCommit(cameras[i]);
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

	OSPRenderer renderer = ospNewRenderer("ao");
	ospSetObject(renderer, "model", world);
	ospSetObject(renderer, "camera", cameras[0]);
	ospSetVec3f(renderer, "bgColor", (osp::vec3f&)vec3f(0.05));
	ospCommit(renderer);

	std::array<OSPFrameBuffer, 2> framebuffers;
	for (size_t i = 0; i < framebuffers.size(); ++i) {
		framebuffers[i] = ospNewFrameBuffer((osp::vec2i&)image_size, OSP_FB_SRGBA,
				OSP_FB_COLOR | OSP_FB_ACCUM);
		ospFrameBufferClear(framebuffers[i], OSP_FB_COLOR | OSP_FB_ACCUM);
	}

	std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> tracked_device_poses;
	bool quit = false;
	uint32_t prev_time = SDL_GetTicks();
	const std::string win_title = "OSPRay + Vive - frame time ";
	while (!quit) {
		vr::VRCompositor()->WaitGetPoses(tracked_device_poses.data(), tracked_device_poses.size(), NULL, 0);
		// TODO: update camera based on HMD position

		const uint32_t cur_time = SDL_GetTicks();
		const float elapsed = (cur_time - prev_time) / 1000.f;
		prev_time = cur_time;
		SDL_Event e;
		while (SDL_PollEvent(&e)){
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)){
				quit = true;
				break;
			}
		}

		// Render each eye and upload them
		for (size_t i = 0; i < framebuffers.size(); ++i) {
			ospSetObject(renderer, "camera", cameras[i]);
			// Debugging test
#if 1
			if (i == 0) {
				ospSetVec3f(renderer, "bgColor", (osp::vec3f&)vec3f(0.1, 0, 0));
			} else {
				ospSetVec3f(renderer, "bgColor", (osp::vec3f&)vec3f(0, 0, 0.1));
			}
#endif

			ospCommit(renderer);
			ospFrameBufferClear(framebuffers[i], OSP_FB_COLOR);
			ospRenderFrame(framebuffers[i], renderer, OSP_FB_COLOR | OSP_FB_ACCUM);
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

		vr::VRCompositor()->Submit(vr::Eye_Left, &left_eye);
		vr::VRCompositor()->Submit(vr::Eye_Right, &right_eye);

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


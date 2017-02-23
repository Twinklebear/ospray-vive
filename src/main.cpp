#include <iostream>
#include <vector>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <ospray.h>
#include <ospcommon/vec.h>
#include "gl_core_3_3.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

static int WIN_WIDTH = 1280;
static int WIN_HEIGHT = 720;

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

	SDL_Window *win = SDL_CreateWindow("OSPRay-vive", SDL_WINDOWPOS_CENTERED,
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
	SDL_GL_SetSwapInterval(1);

	GLuint fbo, texture;
	glGenFramebuffers(1, &fbo);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WIN_WIDTH, WIN_HEIGHT, 0, GL_RGBA,
			GL_UNSIGNED_BYTE, nullptr);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	ospInit(&argc, argv);

	using namespace ospcommon;
	const vec2i imageSize(WIN_WIDTH, WIN_HEIGHT);
	const vec3f camPos(0, 0, 6);
	const vec3f camDir = vec3f(0) - camPos;
	const vec3f camUp(0, 1, 0);

	OSPCamera camera = ospNewCamera("perspective");
	ospSetf(camera, "aspect", imageSize.x / static_cast<float>(imageSize.y));
	ospSetVec3f(camera, "pos", (osp::vec3f&)camPos);
	ospSetVec3f(camera, "dir", (osp::vec3f&)camDir);
	ospSetVec3f(camera, "up",  (osp::vec3f&)camUp);
	ospCommit(camera);

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
	OSPData normal_data = nullptr;
	OSPData tex_data = nullptr;
	OSPData pos_data = ospNewData(attrib.vertices.size() / 3, OSP_FLOAT3,
			attrib.vertices.data(), OSP_DATA_SHARED_BUFFER);
	ospCommit(pos_data);
	if (!attrib.normals.empty()) {
		normal_data = ospNewData(attrib.normals.size() / 3, OSP_FLOAT3,
				attrib.vertices.data(), OSP_DATA_SHARED_BUFFER);
		ospCommit(normal_data);
	}
	if (!attrib.texcoords.empty()) {
		tex_data = ospNewData(attrib.texcoords.size() / 2, OSP_FLOAT2,
				attrib.vertices.data(), OSP_DATA_SHARED_BUFFER);
		ospCommit(tex_data);
	}

	for (size_t s = 0; s < shapes.size(); ++s) {
		std::cout << "Loading mesh " << shapes[s].name
			<< ", has " << shapes[s].mesh.indices.size() << " vertices\n";
		const tinyobj::mesh_t &mesh = shapes[s].mesh;
		std::vector<int32_t> indices;
		indices.reserve(mesh.indices.size() * 3);
		for (const auto &idx : mesh.indices) {
			indices.push_back(idx.vertex_index);
		}
		OSPData idx_data = ospNewData(indices.size() / 3, OSP_INT3, indices.data());
		ospCommit(idx_data);
		OSPGeometry geom = ospNewGeometry("triangles");
		ospSetObject(geom, "vertex", pos_data);
		ospSetObject(geom, "index", idx_data);
		if (normal_data) {
			//ospSetObject(geom, "vertex.normal", normal_data);
		}
		if (tex_data) {
			//ospSetObject(geom, "vertex.texcoord", tex_data);
		}
		ospCommit(geom);
		ospAddGeometry(world, geom);
	}
	ospCommit(world);

	OSPRenderer renderer = ospNewRenderer("ao");
	ospSetObject(renderer, "model", world);
	ospSetObject(renderer, "camera", camera);
	ospSetVec3f(renderer, "bgColor", (osp::vec3f&)vec3f(0.05, 0.05, 0.05));
	ospCommit(renderer);

	OSPFrameBuffer framebuffer = ospNewFrameBuffer((osp::vec2i&)imageSize, OSP_FB_SRGBA,
			OSP_FB_COLOR | OSP_FB_ACCUM);
	ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);

	bool quit = false;
	while (!quit){
		SDL_Event e;
		while (SDL_PollEvent(&e)){
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)){
				quit = true;
				break;
			}
		}
		ospFrameBufferClear(framebuffer, OSP_FB_COLOR);
		ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR | OSP_FB_ACCUM);
		const uint32_t *fb = static_cast<const uint32_t*>(ospMapFrameBuffer(framebuffer, OSP_FB_COLOR));
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, WIN_WIDTH, WIN_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, fb);
		ospUnmapFrameBuffer(fb, framebuffer);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, WIN_WIDTH, WIN_HEIGHT, 0, 0, WIN_WIDTH, WIN_HEIGHT,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);

		SDL_GL_SwapWindow(win);
	}

	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}


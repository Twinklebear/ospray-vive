#include <iostream>
#include <cassert>
#include <iomanip>
#include <SDL.h>
#include "gl_debug.h"

void log_debug_msg(GLenum src, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar *msg){
	//Print a time stamp for the message
	float sec = SDL_GetTicks() / 1000.f;
	int min = static_cast<int>(sec / 60.f);
	sec -= sec / 60.f;
	std::cout << "[" << min << ":"
		<< std::setprecision(3) << sec << "] OpenGL Debug -";
	switch (severity){
	case GL_DEBUG_SEVERITY_HIGH_ARB:
		std::cout << " High severity";
		break;
	case GL_DEBUG_SEVERITY_MEDIUM_ARB:
		std::cout << " Medium severity";
		break;
	case GL_DEBUG_SEVERITY_LOW_ARB:
		std::cout << " Low severity";
	}
	switch (src){
	case GL_DEBUG_SOURCE_API_ARB:
		std::cout << " API";
		break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
		std::cout << " Window system";
		break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
		std::cout << " Shader compiler";
		break;
	case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
		std::cout << " Third party";
		break;
	case GL_DEBUG_SOURCE_APPLICATION_ARB:
		std::cout << " Application";
		break;
	default:
		std::cout << " Other";
	}
	switch (type){
	case GL_DEBUG_TYPE_ERROR_ARB:
		std::cout << " Error";
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
		std::cout << " Deprecated behavior";
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
		std::cout << " Undefined behavior";
		break;
	case GL_DEBUG_TYPE_PORTABILITY_ARB:
		std::cout << " Portability";
		break;
	case GL_DEBUG_TYPE_PERFORMANCE_ARB:
		std::cout << " Performance";
		break;
	default:
		std::cout << " Other";
	}
	std::cout << ":\n\t" << msg << "\n";
	// Break for a stack trace of sorts
	assert(severity != GL_DEBUG_SEVERITY_HIGH_ARB && type != GL_DEBUG_TYPE_ERROR_ARB);
}
#ifdef _WIN32
void APIENTRY debug_callback(GLenum src, GLenum type, GLuint id, GLenum severity,
	GLsizei len, const GLchar *msg, const GLvoid*)
{
	log_debug_msg(src, type, id, severity, len, msg);
}
#else
void debug_callback(GLenum src, GLenum type, GLuint id, GLenum severity,
	GLsizei len, const GLchar *msg, const GLvoid*)
{
	log_debug_msg(src, type, id, severity, len, msg);
}
#endif
void register_debug_callback(){
	if (ogl_ext_ARB_debug_output){
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		glDebugMessageCallbackARB(debug_callback, nullptr);
		glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		glDebugMessageInsertARB(GL_DEBUG_SOURCE_APPLICATION_ARB, GL_DEBUG_TYPE_OTHER_ARB,
				0, GL_DEBUG_SEVERITY_LOW_ARB, 16, "DEBUG LOG START");
	}
}


cmake_minimum_required(VERSION 3.0)
project(OSP_VIVE)

# Use modified FindSDL2 module
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${OSP_VIVE_SOURCE_DIR}/cmake")

# Bump up warning levels appropriately for each compiler
if (UNIX OR APPLE OR MINGW)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++14 -m64")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE} -O3")
elseif (MSVC)
	add_definitions(-DNOMINMAX)
	if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W3" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /Zi")
	endif()
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
endif()

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(ospray REQUIRED)
find_package(OpenVR REQUIRED)

include_directories(${SDL2_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR}
	${OSPRAY_INCLUDE_DIRS} ${OPENVR_INCLUDE_DIR})
add_subdirectory(src)


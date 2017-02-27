include_directories(${PROJECT_BINARY_DIR})
include_directories_ispc(${PROJECT_BINARY_DIR}/ospray/)
include_directories_ispc(${PROJECT_BINARY_DIR}../../ospray)

ospray_create_library(ospray_module_vive
	ospray/vr_camera.cpp
	ospray/vr_camera.ispc
	ospray/vive_module.cpp
	LINK
	ospray)

ospray_create_application(ospray-vive
	main.cpp
	gl_debug.cpp
	gl_core_3_3.c
	LINK
	ospray
	${SDL2_LIBRARY}
	${OPENGL_LIBRARIES}
	${OPENVR_LIBRARY})


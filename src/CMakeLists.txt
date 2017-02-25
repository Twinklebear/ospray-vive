add_executable(ospray-vive main.cpp gl_debug.cpp gl_core_3_3.c)
target_link_libraries(ospray-vive ${SDL2_LIBRARY} ${OPENGL_LIBRARIES}
	${OSPRAY_LIBRARIES} ${OPENVR_LIBRARY})


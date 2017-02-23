#pragma once

#include "gl_core_3_3.h"

/*
 * Register the debug callback using the context capabilities to select between 4.3+ core debug
 * and ARB debug
 */
void register_debug_callback();


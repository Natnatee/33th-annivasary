#ifndef _ANNIVERSARY_33_INFERENCE_H
#define _ANNIVERSARY_33_INFERENCE_H

#include <Arduino.h>
#include <stdarg.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef round
#undef round
#endif
#ifdef DEFAULT
#undef DEFAULT
#endif

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "edge-impulse-sdk/dsp/numpy.hpp"
#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/classifier/ei_classifier_smooth.h"

extern void ei_printf(const char *format, ...);

#endif // _ANNIVERSARY_33_INFERENCE_H

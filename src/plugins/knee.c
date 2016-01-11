#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Knee";
const char* plugin_persistence_name = "mjack_knee";
const unsigned plugin_ladspa_unique_id = 18;

#define KNOBS \
  X(CC_KNEE, 81, "Knee") \
  X(CC_SHAPE, 82, "Shape")

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};

struct filter {
  float *in;
  float *out;
};

static float shape2(float x) {
  return x / sqrtf(1.0f + x * x);
}

void plugin_process(struct instance* instance, int nframes) {
  struct filter *h = instance->plugin;
  double knee = pow((1 + instance->wrapper_cc[CC_KNEE])/128.0, 2);
  double knee_recip = 1.0 / knee;
  if (instance->wrapper_cc[CC_SHAPE] < 64) {
    FOR(i, nframes) { h->out[i] = tanh(h->in[i] * knee_recip) * knee; }
  } else {
    FOR(i, nframes) { h->out[i] = shape2(h->in[i] * knee_recip) * knee; }
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct filter *h = calloc(1, sizeof(struct filter));
  instance->plugin = h;
  wrapper_add_audio_input(instance, "in", &h->in);
  wrapper_add_audio_output(instance, "out", &h->out);
#define X(name, value, label) wrapper_add_cc(instance, value, label, #name, 64);
  KNOBS
#undef X
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  instance->plugin = NULL;
}

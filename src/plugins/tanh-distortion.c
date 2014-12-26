#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "TanhDistortion";
const char* plugin_persistence_name = "mjack_tanh_distortion";
const unsigned plugin_ladspa_unique_id = 6;

#define KNOBS \
  X(CC_DRIVE, 81, "Drive") \
  X(CC_GAIN, 82, "Gain")

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};

struct filter {
  float *in;
  float *out;
};

void plugin_process(struct instance* instance, int nframes) {
  struct filter *h = instance->plugin;
  double drive = 16 * pow((1 + instance->wrapper_cc[CC_DRIVE])/128.0, 2);
  double gain = pow((1 + instance->wrapper_cc[CC_GAIN])/128.0, 2);

  FOR(i, nframes) {
    h->out[i] = tanh(h->in[i] * drive) * gain;
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

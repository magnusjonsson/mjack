#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Slew";
const char* plugin_persistence_name = "mjack_slew";
const unsigned plugin_ladspa_unique_id = 22;

#define KNOBS \
  X(CC_SLEWRATE, 81, "Slew rate") \

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};

struct filter {
  float dt;
  float x;
  float *in;
  float *out;
};

static float slew(float x) {
  return x / sqrtf(1.0f + x * x);
}

void plugin_process(struct instance* instance, int nframes) {
  struct filter *h = instance->plugin;
  float rate = h->dt * 10 * pow(40000 / 10.0, (instance->wrapper_cc[CC_SLEWRATE])/127.0);
  float k1 = 1 / rate;
  float k2 = rate;
  FOR(i, nframes) {
    h->out[i] = h->x += k2 * slew(k1 * (h->in[i] - h->x));
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct filter *h = calloc(1, sizeof(struct filter));
  h->dt = 1 / sample_rate;
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

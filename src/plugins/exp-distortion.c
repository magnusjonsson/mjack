#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "ExpDistortion";
const char* plugin_persistence_name = "mjack_exp_distortion";
const unsigned plugin_ladspa_unique_id = 15;

#define KNOBS \
  X(CC_DRIVE, 81, "Drive") \
  X(CC_GAIN, 82, "Gain") \
  X(CC_BIAS, 83, "Bias") \
  X(CC_ATTACK, 84, "Attack") \

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};

struct filter {
  float *in;
  float *out;
  float dt;
  float fb_integral;
};

void plugin_process(struct instance* instance, int nframes) {
  struct filter *h = instance->plugin;
  float drive = 16 * pow((1 + instance->wrapper_cc[CC_DRIVE])/128.0, 4);
  float gain = pow((1 + instance->wrapper_cc[CC_GAIN])/128.0, 2);
  float bias = (instance->wrapper_cc[CC_BIAS] - 63.5) / 64.0;
  float attack = 1000 * pow((1 + instance->wrapper_cc[CC_ATTACK])/128.0, 3);

  FOR(i, nframes) {
    float out = tanhf(h->in[i] * drive + h->fb_integral) + bias;
    h->fb_integral -= out * fminf(0.001, attack * h->dt);
    h->out[i] = out * gain;
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct filter *h = calloc(1, sizeof(struct filter));
  h->dt = 1.0 / sample_rate;
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

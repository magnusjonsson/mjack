#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "X2Distortion";
const char* plugin_persistence_name = "mjack_x2_distortion";
const unsigned plugin_ladspa_unique_id = 20;

#define KNOBS \
  X(CC_DRIVE, 81, "Drive") \
  X(CC_GAIN, 82, "Gain") \
  X(CC_BIAS, 83, "Bias") \
  X(CC_LFO_FREQ, 84, "LFO Freq") \
  X(CC_LFO_DEPTH, 85, "LFO Depth") \

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};

struct filter {
  float *in;
  float *out;
  float dt;
  float hpstate;
  float phase;
};

void plugin_process(struct instance* instance, int nframes) {
  struct filter *h = instance->plugin;
  float drive = 16 * pow((1 + instance->wrapper_cc[CC_DRIVE])/128.0, 2);
  float gain = pow((1 + instance->wrapper_cc[CC_GAIN])/128.0, 2);
  float bias = instance->wrapper_cc[CC_BIAS] / 127.0;
  float phaseinc = instance->wrapper_cc[CC_LFO_FREQ] * 20.0 / 128.0 * h->dt;
  float depth = instance->wrapper_cc[CC_LFO_DEPTH] * 4.0 / 128.0;
  float hpcoeff = 50 * h->dt;

  float phase = h->phase;
  float hpstate = h->hpstate;
  float * restrict in = h->in;
  float * restrict out = h->out;

  FOR(i, nframes) {
    float x = in[i] * drive + bias + sinf(phase * 2 * 3.141592) * depth;
    float x2 = x * x;
    float y = x2 / (1 + x2);
    y -= hpstate;
    out[i] = y * gain;
    hpstate += y * hpcoeff;
    phase += phaseinc;
    if (phase > 0.5) {
      phase -= 1;
    }
  }
  h->hpstate = hpstate;
  h->phase = phase;
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct filter *f = calloc(1, sizeof(struct filter));
  f->dt = 1 / sample_rate;
  instance->plugin = f;
  wrapper_add_audio_input(instance, "in", &f->in);
  wrapper_add_audio_output(instance, "out", &f->out);
#define X(name, value, label) wrapper_add_cc(instance, value, label, #name, 64);
  KNOBS
#undef X
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  instance->plugin = NULL;
}

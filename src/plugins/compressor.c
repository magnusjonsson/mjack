#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Compressor";
const char* plugin_persistence_name = "mjack_compressor";
const unsigned plugin_ladspa_unique_id = 14;

#define KNOBS \
  X(CC_ATTACK, 81, "Attack") \
  X(CC_RELEASE, 82, "Release") \
  X(CC_THRESHOLD, 83, "Threshold") \

#define MIN_ATTACK 0.0001
#define MAX_ATTACK 10
#define MIN_RELEASE 0.0001
#define MAX_RELEASE 10

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};

struct compressor {
  float *in;
  float *out;
  double power;
  double dt;
};

void plugin_process(struct instance* instance, int nframes) {
  struct compressor *c = instance->plugin;
  double k_attack = c->dt / (MIN_ATTACK * pow(MAX_ATTACK / MIN_ATTACK, instance->wrapper_cc[CC_ATTACK] / 127.0));
  double k_release = c->dt / (MIN_RELEASE * pow(MAX_RELEASE / MIN_RELEASE, instance->wrapper_cc[CC_RELEASE] / 127.0));
  double t = pow((1 + instance->wrapper_cc[CC_THRESHOLD]) / 128.0, 2);

  FOR(i, nframes) {
    float x = c->in[i];
    double instantaneous_power = x * x;
    double k = instantaneous_power > c->power ? k_attack : k_release;
    c->power += k * (instantaneous_power - c->power);
    float gain = t / sqrtf(t*t + c->power + 1e-30f);
    c->out[i] = x * gain;
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  printf("Compressor init\n");
  struct compressor *c = calloc(1, sizeof(struct compressor));
  instance->plugin = c;
  c->dt = 1 / sample_rate;
  wrapper_add_audio_input(instance, "in", &c->in);
  wrapper_add_audio_output(instance, "out", &c->out);
#define X(name, value, label) wrapper_add_cc(instance, value, label, #name, 64);
  KNOBS
#undef X
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  instance->plugin = NULL;
}

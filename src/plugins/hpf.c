#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "HPF";
const char* plugin_persistence_name = "mjack_hpf";
const unsigned plugin_ladspa_unique_id = 4;

#define CC_CUTOFF 80
#define CC_ORDER 81

#define MAX_ORDER 4

struct hpf {
  float *in;
  float *out;

  double dt;

  double state[MAX_ORDER];
};

static void init(struct hpf* h, double sample_rate) {
  FOR(j, MAX_ORDER) h->state[j] = 0.0;
  h->dt = 1 / sample_rate;
}

void plugin_process(struct instance* instance, int nframes) {
  struct hpf *h = instance->plugin;
  double w = 2 * 3.141592 * 10.0 * pow(40000.0/10.0, instance->wrapper_cc[CC_CUTOFF] / 127.0);
  int order = 1 + instance->wrapper_cc[CC_ORDER] * MAX_ORDER / 128;
  double k = -expm1(-w * h->dt / sqrt(order));
  FOR(i, nframes) {
    double s = h->in[i];
    FOR(j, MAX_ORDER) {
      if (j < order) {
	double d = s - h->state[j];
	h->state[j] += 0.5 * k * d;
	s -= h->state[j];
	h->state[j] += 0.5 * k * d;
      } else {
	h->state[j] = 0;
      }
    }
    h->out[i] = s;
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct hpf *h = calloc(1, sizeof(struct hpf));
  instance->plugin = h;
  init(h, sample_rate);
  wrapper_add_cc(instance, CC_CUTOFF, "Cutoff", "cutoff", 64);
  wrapper_add_cc(instance, CC_ORDER, "Order", "Order", 64);
  wrapper_add_audio_input(instance, "in", &h->in);
  wrapper_add_audio_output(instance, "out", &h->out);
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  instance->plugin = NULL;
}

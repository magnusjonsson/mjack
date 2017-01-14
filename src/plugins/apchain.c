#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <math.h>
#include <malloc.h>
#include "../dsp/ap.h"
#include "../wrappers/wrapper.h"

const char* plugin_name = "APChain";
const char* plugin_persistence_name = "mjack_apchain";
const unsigned plugin_ladspa_unique_id = 2;

#define MAX_STAGES 128
#define FOR(var,limit) for(int var = 0; var < limit; ++var)
#define CC_CUTOFF 77
#define CC_STAGES 78

struct apchain {
  float *inbuf;
  float *outbuf;

  double dt;

  double apstate[MAX_STAGES];
};

static void init(struct apchain* a, double nframes_per_second) {
  a->dt = 1.0 / nframes_per_second;
}

void plugin_process(struct instance* instance, int nframes) {
  struct apchain *a = instance->plugin;
  double freq = 440.0 * pow(2.0, (instance->wrapper_cc[CC_CUTOFF] - 69) / 12.0);
  double k = ap_coeff(freq * a->dt);
  int num_stages = 1 + instance->wrapper_cc[CC_STAGES];
  FOR(f, nframes) {
    double x = a->inbuf[f] + 1e-12;
    FOR(s, num_stages) {
      x = ap_tick(&a->apstate[s], x, k);
    }
    a->outbuf[f] = x;
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  printf("Apchain init\n");
  struct apchain *a = calloc(1, sizeof(struct apchain));
  instance->plugin = a;
  init(a, sample_rate);
  wrapper_add_audio_input(instance, "in", &a->inbuf);
  wrapper_add_audio_output(instance, "out", &a->outbuf);
  wrapper_add_cc(instance, CC_CUTOFF, "Cutoff", "cutoff", 64);
  wrapper_add_cc(instance, CC_STAGES, "Stages", "stages", 31);
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  instance->plugin = NULL;
}

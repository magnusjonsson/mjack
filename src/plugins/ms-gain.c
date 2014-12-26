#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Mid-Side Gain";
const char* plugin_persistence_name = "mjack_ms_gain";
const unsigned plugin_ladspa_unique_id = 11;

#define CC_MID_GAIN 80
#define CC_SIDE_GAIN 81

struct ms_gain {
  float *in[2];
  float *out[2];
};

void plugin_process(struct instance* instance, int nframes) {
  struct ms_gain *p = instance->plugin;
  float mid_gain = powf(instance->wrapper_cc[CC_MID_GAIN]/64.0f, 2.0f);
  float side_gain = powf(instance->wrapper_cc[CC_SIDE_GAIN]/64.0f, 2.0f);
  FOR(i, nframes) {
    float mid = (p->in[0][i] + p->in[1][i]) * (0.5f * mid_gain);
    float side = (p->in[0][i] - p->in[1][i]) * (0.5f * side_gain);
    p->out[0][i] = mid + side;
    p->out[1][i] = mid - side;
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct ms_gain *h = calloc(1, sizeof(struct ms_gain));
  instance->plugin = h;
  wrapper_add_cc(instance, CC_MID_GAIN, "Mid Gain", "mid_gain", 64);
  wrapper_add_cc(instance, CC_SIDE_GAIN, "Side Gain", "side_gain", 64);
  wrapper_add_audio_input(instance, "in left", &h->in[0]);
  wrapper_add_audio_input(instance, "in right", &h->in[1]);
  wrapper_add_audio_output(instance, "out left", &h->out[0]);
  wrapper_add_audio_output(instance, "out right", &h->out[1]);
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  instance->plugin = NULL;
}

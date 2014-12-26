#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "MonoPanner";
const char* plugin_persistence_name = "mjack_mono_panner";
const unsigned plugin_ladspa_unique_id = 7;

#define KNOBS \
  X(CC_SIDE_GAIN, 81, "Side Gain") \
  X(CC_SIDE_DELAY, 82, "Side Delay") \
  X(CC_SIDE_HP_CUTOFF, 83, "Side HP Cutoff")

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};

#define DELAY_BUFFER_LEN 65536

struct filter {
  float *in;
  float *outL;
  float *outR;
  double dt;
  double lp;
  float delay_buffer[DELAY_BUFFER_LEN];
  int index;
};

void plugin_process(struct instance* instance, int nframes) {
  struct filter *h = instance->plugin;
  double gain = (instance->wrapper_cc[CC_SIDE_GAIN] - 63.5) / 63.5;

  double hp_omega = 2 * 3.141592 * 440 * pow(2.0, (instance->wrapper_cc[CC_SIDE_HP_CUTOFF] - 69) / 12.0);
  double hp_k = -expm1(-hp_omega * h->dt);
  int delay = (int) (0.5 * pow(instance->wrapper_cc[CC_SIDE_DELAY] / 127.0, 2) / h->dt + 0.5);

  FOR(i, nframes) {
    double mid = h->in[i];

    h->delay_buffer[h->index] = mid;
    double side = h->delay_buffer[(h->index - delay) & (DELAY_BUFFER_LEN - 1)];
    h->index++;
    h->index &= (DELAY_BUFFER_LEN - 1);

    h->lp += hp_k * (side - h->lp);
    side -= h->lp;
    side *= gain;
    h->outL[i] = mid + side;
    h->outR[i] = mid - side;
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct filter *h = calloc(1, sizeof(struct filter));
  instance->plugin = h;
  h->dt = 1.0 / sample_rate;
  wrapper_add_audio_input(instance, "in", &h->in);
  wrapper_add_audio_output(instance, "left", &h->outL);
  wrapper_add_audio_output(instance, "right", &h->outR);
#define X(name, value, label) wrapper_add_cc(instance, value, label, #name, 64);
  KNOBS
#undef X
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  instance->plugin = NULL;
}

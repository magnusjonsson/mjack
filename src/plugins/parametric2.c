#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../wrappers/wrapper.h"
#include "../dsp/biquad.h"

const char* plugin_name = "Parametric2";
const char* plugin_persistence_name = "mjack_parametric2";
const unsigned plugin_ladspa_unique_id = 16;

#define KNOBS \
  X(CC_FREQ, 81, "Freq") \
  X(CC_Q, 82, "Q") \
  X(CC_LOW_GAIN, 83, "Low Gain") \
  X(CC_MID_GAIN, 84, "Mid Gain") \
  X(CC_HIGH_GAIN, 85, "High Gain")

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};

struct mjack_biquad {
  float *in;
  float *out;

  double dt;
  struct biquad_state biquad_state;
};

static void init(struct mjack_biquad *m, double sample_rate) {
  memset(m, 0, sizeof(*m));
  m->dt = 1 / sample_rate;
}

static double cc_gain(double cc) {
  double f = cc / 127.0;
  return (1 + f) / (2 - f);
}

void plugin_process(struct instance* instance, int nframes) {
  struct mjack_biquad *m = instance->plugin;

  double f = 440 * pow(2.0, (instance->wrapper_cc[CC_FREQ] - 69) / 12.0);

  struct biquad_params params = {
    .w = 2 * 3.141592 * f * m->dt,
    .Q = (1 + instance->wrapper_cc[CC_Q]) * 4 / 128.0,
    .g2 = cc_gain(instance->wrapper_cc[CC_HIGH_GAIN]),
    .g1 = cc_gain(instance->wrapper_cc[CC_MID_GAIN]),
    .g0 = cc_gain(instance->wrapper_cc[CC_LOW_GAIN]),
  };

  struct biquad_coeffs coeffs = biquad_digital_parametric(params);

  biquad_process(coeffs, &m->biquad_state, m->in, m->out, nframes);
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct mjack_biquad *m = calloc(1, sizeof(struct mjack_biquad));
  instance->plugin = m;
  init(m, sample_rate);
  wrapper_add_audio_input(instance, "in", &m->in);
  wrapper_add_audio_output(instance, "out", &m->out);
#define X(name, value, label) wrapper_add_cc(instance, value, label, #name, 64);
  KNOBS
#undef X
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  instance->plugin = NULL;
}

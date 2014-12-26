#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Parametric";
const char* plugin_persistence_name = "mjack_parametric";
const unsigned plugin_ladspa_unique_id = 5;

#define KNOBS \
  X(CC_FREQ, 81, "Freq") \
  X(CC_Q, 82, "Q") \
  X(CC_GAIN, 83, "Gain")

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};

struct filter {
  float *in;
  float *out;

  double x1, x2;
  double y1, y2;

  double dt;
};

static void init(struct filter* h, double sample_rate) {
  h->x1 = 0;
  h->x2 = 0;
  h->y1 = 0;
  h->y2 = 0;
  h->dt = 1 / sample_rate;
}

void plugin_process(struct instance* instance, int nframes) {
  struct filter *h = instance->plugin;
  double w0 = 2 * 3.141592 * 440 * pow(2.0, (instance->wrapper_cc[CC_FREQ] - 69) / 12.0) * h->dt;
  double Q = (1 + instance->wrapper_cc[CC_Q]) * 8 / 128.0;

  double sqrt_gain = (1 + instance->wrapper_cc[CC_GAIN]) * 2 / 128.0;

  double A = sqrt_gain;

  double alpha = sin(w0)/(2*Q);

  double b0 =  1 + alpha*A;
  double b1 = -2*cos(w0);
  double b2 =  1 - alpha*A;
  double a0 =  1 + alpha/A;
  double a1 = -2*cos(w0);
  double a2 =  1 - alpha/A;

  FOR(i, nframes) {
    double x0 = h->in[i];
    double y0 =
      (1 / a0)
      * (+ b0 * x0
	 + b1 * h->x1
	 + b2 * h->x2
	 - a1 * h->y1
	 - a2 * h->y2);
    h->out[i] = y0;
    h->x2 = h->x1; h->x1 = x0;
    h->y2 = h->y1; h->y1 = y0;
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct filter *h = calloc(1, sizeof(struct filter));
  instance->plugin = h;
  init(h, sample_rate);
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

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Biquad";
const char* plugin_persistence_name = "mjack_biquad";
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

struct biquad_coeffs {

  // b2 s^2 + b1 s + b0
  // ------------------
  // a2 s^2 + a1 s + a0


  // b2 z^-2 + b1 z^-1 + b0
  // ------------------
  // a2 z^-2 + a1 z^-1 + a0

  double b2, b1, b0;
  double a2, a1, a0;
};

struct biquad_params {
  double w, Q; // mid frequency and quality factor
  double g2, g1, g0; // square root of high, mid, low gain
};

inline struct biquad_params biquad_prewarp(struct biquad_params _) {
  double t = tan(0.5 * fmin(3.14, _.w));
  return (struct biquad_params) {
    .w = t, .Q = _.Q / (1.0 + t * t),
    .g2 = _.g2, .g1 = _.g1, .g0 = _.g0,
  };
};

inline struct biquad_coeffs biquad_analog_parametric(struct biquad_params _) {
  return (struct biquad_coeffs) {
    .b2 =       _.g2, .b1 = _.w / _.Q * _.g1, .b0 = _.w * _.w * _.g0,
    .a2 = 1.0 / _.g2, .a1 = _.w / _.Q / _.g1, .a0 = _.w * _.w / _.g0,
  };
}

inline struct biquad_coeffs biquad_bilinear_transform(struct biquad_coeffs _) {
  return (struct biquad_coeffs) {
    .b2 = _.b2 - _.b1 + _.b0, .b1 = 2.0 * (_.b0 - _.b2), .b0 = _.b2 + _.b1 + _.b0,
    .a2 = _.a2 - _.a1 + _.a0, .a1 = 2.0 * (_.a0 - _.a2), .a0 = _.a2 + _.a1 + _.a0,
   };
};

inline struct biquad_coeffs biquad_digital_parametric(struct biquad_params p) {
  return biquad_bilinear_transform(biquad_analog_parametric(biquad_prewarp(p)));
}

struct biquad_state {
  double x1, x2;
  double y1, y2;
};

static void biquad_process(struct biquad_coeffs c, struct biquad_state *s, const float *in, float *out, int n) {
  FOR(i, n) {
    double x0 = in[i];
    double y0 =
      (1 / c.a0)
      * (+ c.b0 * x0
	 + c.b1 * s->x1
	 + c.b2 * s->x2
	 - c.a1 * s->y1
	 - c.a2 * s->y2);
    out[i] = y0;
    s->x2 = s->x1; s->x1 = x0;
    s->y2 = s->y1; s->y1 = y0;
  }
}

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

  printf("f %g Q %g g2 %g g1 %g g0 %g\n", f, params.Q, params.g2, params.g1, params.g0);

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

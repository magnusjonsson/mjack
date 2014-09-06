#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include "wrapper.h"

const char* plugin_name = "Reverb";
const char* plugin_persistence_name = "mjack_reverb";
const unsigned plugin_ladspa_unique_id = 3;

#define NUM_INS 2
#define NUM_OUTS 2

#define NUM_STAGES 8
#define MIX_SIZE 4

#define CC_WET_LEVEL 91
#define CC_FEEDBACK 72
#define CC_DECAY 80
#define CC_DAMPING 81
#define CC_STAGES 127 // TODO assign

#define BUF_LEN 32


struct reverb {
  float* inbufs[NUM_INS];
  float* outbufs[NUM_OUTS];
  float buf[NUM_STAGES][MIX_SIZE][BUF_LEN];
  float buf_offs[NUM_STAGES][MIX_SIZE];
  double dt;
  float* tank_buf;
  int tank_len;
  int base;
};
#define SQRT_ONE_HALF 0.707106781

static double frand(void) {
  return rand() / (RAND_MAX + 1.0);
}

static int irand(int max_plus_1) {
  int i = (int) (frand() * max_plus_1);
  return i;
}

static void init_buf_offs(struct reverb* r) {
  srand(1053);
  FOR(s, NUM_STAGES) {
    FOR(m, MIX_SIZE) {
      r->buf_offs[s][m] = r->tank_len/NUM_STAGES * s + r->tank_len/NUM_STAGES/MIX_SIZE*m + irand(r->tank_len/NUM_STAGES/MIX_SIZE - BUF_LEN);
    }
  }
  FOR(s, NUM_STAGES / 2) {
    // make sure 1st reflection arrives at the same time in both speakers
    r->buf_offs[NUM_STAGES/2 + s][0         ] = r->tank_len/2 + r->buf_offs[s][0         ];
    r->buf_offs[NUM_STAGES/2 + s][MIX_SIZE-1] = r->tank_len/2 + r->buf_offs[s][MIX_SIZE-1];
  }
}

static void init(struct reverb* r, double nframes_per_second) {
  r->dt = 1.0 / nframes_per_second;
  r->tank_len = (int)(nframes_per_second * 1.0);
  r->tank_buf = calloc(r->tank_len, sizeof(float)); // TODO don't leak
  init_buf_offs(r);
}

static double z[NUM_STAGES];

static void mix4(struct reverb* r, int s, int n, double k, double a) {
  FOR(i, n) {
    float t0 = r->buf[s][0][i];
    float t1 = r->buf[s][1][i];
    float t2 = r->buf[s][2][i];
    float t3 = r->buf[s][3][i];
    double T = (t0 + t1 + t2 + t3) * 0.25;
    z[s] += (T - z[s]) * a;
    r->buf[s][0][i] = t0 - T - z[s] * k;
    r->buf[s][1][i] = t1 - T - z[s] * k;
    r->buf[s][2][i] = t2 - T - z[s] * k;
    r->buf[s][3][i] = t3 - T - z[s] * k;
  }
}

static void mix8(struct reverb* r, int s, int n, double k) {
  k *= 0.125;
  FOR(i, n) {
    double t0 = r->buf[s][0][i];
    double t1 = r->buf[s][1][i];
    double t2 = r->buf[s][2][i];
    double t3 = r->buf[s][3][i];
    double t4 = r->buf[s][4][i];
    double t5 = r->buf[s][5][i];
    double t6 = r->buf[s][6][i];
    double t7 = r->buf[s][7][i];
    double m = (t0 + t1 + t2 + t3 + t4 + t5 + t6 + t7) * k;
    r->buf[s][0][i] = t0 - m;
    r->buf[s][1][i] = t1 - m;
    r->buf[s][2][i] = t2 - m;
    r->buf[s][3][i] = t3 - m;
    r->buf[s][4][i] = t4 - m;
    r->buf[s][5][i] = t5 - m;
    r->buf[s][6][i] = t6 - m;
    r->buf[s][7][i] = t7 - m;
  }
}

static void mix16(struct reverb* r, int s, int n, double k) {
  k *= 0.0625;
  FOR(i, n) {
    double t0 = r->buf[s][0][i];
    double t1 = r->buf[s][1][i];
    double t2 = r->buf[s][2][i];
    double t3 = r->buf[s][3][i];
    double t4 = r->buf[s][4][i];
    double t5 = r->buf[s][5][i];
    double t6 = r->buf[s][6][i];
    double t7 = r->buf[s][7][i];
    double t8 = r->buf[s][8][i];
    double t9 = r->buf[s][9][i];
    double t10 = r->buf[s][10][i];
    double t11 = r->buf[s][11][i];
    double t12 = r->buf[s][12][i];
    double t13 = r->buf[s][13][i];
    double t14 = r->buf[s][14][i];
    double t15 = r->buf[s][15][i];
    double m = (t0 + t1 + t2 + t3 + t4 + t5 + t6 + t7 + t8 + t9 + t10 + t11 + t12 + t13 + t14 + t15) * k;
    r->buf[s][0][i] = t0 - m;
    r->buf[s][1][i] = t1 - m;
    r->buf[s][2][i] = t2 - m;
    r->buf[s][3][i] = t3 - m;
    r->buf[s][4][i] = t4 - m;
    r->buf[s][5][i] = t5 - m;
    r->buf[s][6][i] = t6 - m;
    r->buf[s][7][i] = t7 - m;
    r->buf[s][8][i] = t8 - m;
    r->buf[s][9][i] = t9 - m;
    r->buf[s][10][i] = t10 - m;
    r->buf[s][11][i] = t11 - m;
    r->buf[s][12][i] = t12 - m;
    r->buf[s][13][i] = t13 - m;
    r->buf[s][14][i] = t14 - m;
    r->buf[s][15][i] = t15 - m;
  }
}

static double square(double x) {
  return x * x;
}

void plugin_process(struct instance* instance, int nframes) {
  struct reverb* r = instance->plugin;
  double reverb_gain = square(instance->wrapper_cc[CC_WET_LEVEL] * (2.0 / 127.0));
  double feedback_gain = -square(instance->wrapper_cc[CC_FEEDBACK] / 127.0);
  double decay_gain = instance->wrapper_cc[CC_DECAY] / 127.0;
  double damping_coeff = instance->wrapper_cc[CC_DAMPING] / 127.0; // TODO sample-rate depending
  int stage = instance->wrapper_cc[CC_STAGES] * (NUM_STAGES / 2) / 128;
  int io_base = 0;
  while (nframes > 0) {
    int n = nframes;
    if (n > BUF_LEN) n = BUF_LEN;
    FOR(s, NUM_STAGES) {
      FOR(m, MIX_SIZE) {
	int j = r->base - r->buf_offs[s][m];
	if (j < 0) {
	  j += r->tank_len;
	}
	if (j + n <= r->tank_len) {
	  memcpy(r->buf[s][m], r->tank_buf + j, n * sizeof(float));
	}
	else {
	  memcpy(r->buf[s][m], r->tank_buf + j, (r->tank_len - j) * sizeof(float));
	  memcpy(r->buf[s][m] + r->tank_len - j, r->tank_buf, (j + n - r->tank_len) * sizeof(float));
	}
      }
    }
    FOR(i, n) {
      double out[NUM_OUTS];
      FOR(o, NUM_OUTS) {
	out[o] = reverb_gain * r->buf[NUM_STAGES/2*o+stage][0][i];
	r->buf[NUM_STAGES/2*o][0][i] *= feedback_gain;
	r->buf[NUM_STAGES/2*o][0][i] += r->inbufs[o][io_base + i];
      }
      FOR(o, NUM_OUTS) {
	r->outbufs[o^1][io_base + i] = out[o];
      }
    }
    if (MIX_SIZE == 4) FOR(s, NUM_STAGES) mix4(r, s, n, decay_gain, damping_coeff);
    if (MIX_SIZE == 8) FOR(s, NUM_STAGES) mix8(r, s, n, 1 + decay_gain);
    if (MIX_SIZE == 16) FOR(s, NUM_STAGES) mix16(r, s, n, 1 + decay_gain);
    FOR(s, NUM_STAGES) {
      FOR(m, MIX_SIZE) {
	int j = r->base - r->buf_offs[s][m];
	if (j < 0) {
	  j += r->tank_len;
	}
	if (j + n <= r->tank_len) {
	  memcpy(r->tank_buf + j, r->buf[s][m], n * sizeof(float));
	}
	else {
	  memcpy(r->tank_buf + j, r->buf[s][m], (r->tank_len - j) * sizeof(float));
	  memcpy(r->tank_buf, r->buf[s][m] + r->tank_len - j, (j + n - r->tank_len) * sizeof(float));
	}
      }
    }
    r->base += n; if (r->base >= r->tank_len) r->base -= r->tank_len;
    io_base += n;
    nframes -= n;
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct reverb* r = memalign(4096, sizeof(struct reverb));
  memset(r, 0, sizeof(struct reverb));
  instance->plugin = r;
  init(r, sample_rate);
#define MAX_NAME_LENGTH 16
  static char inname[NUM_INS][MAX_NAME_LENGTH];
  static char outname[NUM_INS][MAX_NAME_LENGTH];
  FOR(i, NUM_INS) snprintf(inname[i], MAX_NAME_LENGTH, "in %i", i);
  FOR(i, NUM_OUTS) snprintf(outname[i], MAX_NAME_LENGTH, "out %i", i);
  FOR(i, NUM_INS) wrapper_add_audio_input(instance, inname[i], &r->inbufs[i]);
  FOR(i, NUM_OUTS) wrapper_add_audio_output(instance, outname[i], &r->outbufs[i]);
  wrapper_add_cc(instance, CC_WET_LEVEL, "Wet", "wet", 64);
  wrapper_add_cc(instance, CC_FEEDBACK, "Feedback", "feedback", 64);
  wrapper_add_cc(instance, CC_DECAY, "Decay", "decay", 64);
  wrapper_add_cc(instance, CC_DAMPING, "Damping", "damping", 64);
  wrapper_add_cc(instance, CC_STAGES, "Stages", "stages", 64);
}

void plugin_destroy(struct instance* instance) {
  struct reverb* r = instance->plugin;
  free(r->tank_buf);
  r->tank_buf = NULL;
  free(instance->plugin);
  instance->plugin = NULL;
}

#include <stdio.h>




#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Reverb2";
const char* plugin_persistence_name = "mjack_reverb2";
const unsigned plugin_ladspa_unique_id = 21;

#define NUM_OUTS 2

#define NUM_STAGES 4
#define MIX_SIZE 4

#define CC_SIZE 127
#define CC_GAIN 100

#define BUF_LEN 32

#define MAX_SIZE_SECONDS 10.0

struct reverb {
  float *inbufs[NUM_OUTS];
  float *outbufs[NUM_OUTS];
  float buf[NUM_OUTS][NUM_STAGES][MIX_SIZE][BUF_LEN];
  float buf_offs[NUM_OUTS][NUM_STAGES][MIX_SIZE];
  float rand[NUM_OUTS][NUM_STAGES][MIX_SIZE];
  float *tank_buf;
  int max_tank_len;
  int tank_len;
  int base;
  float sample_rate;
};

static int tank_len(float sample_rate, float size_seconds) {
  int n = sample_rate * size_seconds / 2 / NUM_STAGES / MIX_SIZE;
  if (n < BUF_LEN) {
    n = BUF_LEN;
  }
  n *= 2 * NUM_STAGES * MIX_SIZE;
  return n;
}

static void init_buf_offs(struct reverb *r, double size_seconds) {
  r->tank_len = tank_len(r->sample_rate, size_seconds);
  FOR(o, NUM_OUTS) {
    FOR(s, NUM_STAGES) {
      FOR(m, MIX_SIZE) {
	r->buf_offs[o][s][m] = r->tank_len/2*o + r->tank_len/2/NUM_STAGES * s + r->tank_len/2/NUM_STAGES/MIX_SIZE*m + (int) (r->rand[o][s][m] * (r->tank_len/2/NUM_STAGES/MIX_SIZE - BUF_LEN));
      }
    }
  }
  FOR(s, NUM_STAGES) {
    // make sure 1st reflection arrives at the same time in both speakers
    r->buf_offs[1][s][0         ] = r->tank_len/2 + r->buf_offs[0][s][0         ];
    r->buf_offs[1][s][MIX_SIZE-1] = r->tank_len/2 + r->buf_offs[0][s][MIX_SIZE-1];
  }
}

static void init(struct reverb *r, double nframes_per_second) {
  r->sample_rate = nframes_per_second;
  r->max_tank_len = tank_len(r->sample_rate, MAX_SIZE_SECONDS);
  r->tank_buf = calloc(r->max_tank_len, sizeof(float));
  srand(1053);
  FOR(o, NUM_OUTS) {
    FOR(s, NUM_STAGES) {
      FOR(m, MIX_SIZE) {
	r->rand[o][s][m] = rand() / (RAND_MAX + 1.0);
      }
    }
  }
}

static float square(float x) {
  return x * x;
}

void plugin_process(struct instance* instance, int nframes) {
  struct reverb *r = instance->plugin;
  float size_seconds = square((1 + instance->wrapper_cc[CC_SIZE]) / 128.0) * MAX_SIZE_SECONDS;
  float gain[NUM_STAGES];
  FOR(i, NUM_STAGES) {
    gain[i] = square(instance->wrapper_cc[CC_GAIN + i] / 127.0);
  }
  int io_base = 0;
  init_buf_offs(r, size_seconds);
  r->base %= r->tank_len;
  while (nframes > 0) {
    int n = nframes;
    if (n > BUF_LEN) n = BUF_LEN;
    FOR(o, NUM_OUTS) {
      FOR(s, NUM_STAGES) {
	FOR(m, MIX_SIZE) {
	  int j = r->base - r->buf_offs[o][s][m];
	  if (j < 0) {
	    j += r->tank_len;
	  }
	  if (j + n <= r->tank_len) {
	    memcpy(r->buf[o][s][m], r->tank_buf + j, n * sizeof(float));
	  }
	  else {
	    memcpy(r->buf[o][s][m], r->tank_buf + j, (r->tank_len - j) * sizeof(float));
	    memcpy(r->buf[o][s][m] + r->tank_len - j, r->tank_buf, (j + n - r->tank_len) * sizeof(float));
	  }
	}
      }
    }
    float out[NUM_OUTS][BUF_LEN];
    FOR(o, NUM_OUTS) {
      FOR(i, n) {
	out[o][i] = 0;
      }
    }
    FOR(o, NUM_OUTS) {
      FOR(s, NUM_STAGES) {
	FOR(i, n) {
	  out[o][i] += r->buf[o][s][0][i] * gain[s];
	}
      }
    }
    FOR(o, NUM_OUTS) {
      FOR(s, NUM_STAGES) {
	FOR(i, n) {
	  float t0 = r->buf[o][s][0][i];
	  float t1 = r->buf[o][s][1][i];
	  float t2 = r->buf[o][s][2][i];
	  float t3 = r->buf[o][s][3][i];
	  float T = (t0 + t1 + t2 + t3) * 0.5;
	  r->buf[o][s][0][i] = t0 - T;
	  r->buf[o][s][1][i] = t1 - T;
	  r->buf[o][s][2][i] = t2 - T;
	  r->buf[o][s][3][i] = t3 - T;
	}
      }
    }
    FOR(o, NUM_OUTS) {
      FOR(i, n) {
	r->buf[o][0][0][i] = r->inbufs[o][io_base + i];
      }
    }
    FOR(o, NUM_OUTS) {
      FOR(i, n) {
	r->outbufs[o^1][io_base + i] = out[o][i];
      }
    }
    FOR(o, NUM_OUTS) {
      FOR(s, NUM_STAGES) {
	FOR(m, MIX_SIZE) {
	  int j = r->base - r->buf_offs[o][s][m];
	  if (j < 0) {
	    j += r->tank_len;
	  }
	  if (j + n <= r->tank_len) {
	    memcpy(r->tank_buf + j, r->buf[o][s][m], n * sizeof(float));
	  }
	  else {
	    memcpy(r->tank_buf + j, r->buf[o][s][m], (r->tank_len - j) * sizeof(float));
	    memcpy(r->tank_buf, r->buf[o][s][m] + r->tank_len - j, (j + n - r->tank_len) * sizeof(float));
	  }
	}
      }
    }
    r->base += n; if (r->base >= r->tank_len) r->base -= r->tank_len;
    io_base += n;
    nframes -= n;
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct reverb *r = memalign(4096, sizeof(struct reverb));
  memset(r, 0, sizeof(struct reverb));
  instance->plugin = r;
  init(r, sample_rate);
#define MAX_NAME_LENGTH 16
  static char inname[NUM_OUTS][MAX_NAME_LENGTH];
  static char outname[NUM_OUTS][MAX_NAME_LENGTH];
  static char gainname1[NUM_STAGES][MAX_NAME_LENGTH];
  static char gainname2[NUM_STAGES][MAX_NAME_LENGTH];
  FOR(i, NUM_OUTS) snprintf(inname[i], MAX_NAME_LENGTH, "in %i", i);
  FOR(i, NUM_OUTS) snprintf(outname[i], MAX_NAME_LENGTH, "out %i", i);
  FOR(i, NUM_STAGES) snprintf(gainname1[i], MAX_NAME_LENGTH, "Gain %i", i);
  FOR(i, NUM_STAGES) snprintf(gainname2[i], MAX_NAME_LENGTH, "gain %i", i);
  FOR(i, NUM_OUTS) wrapper_add_audio_input(instance, inname[i], &r->inbufs[i]);
  FOR(i, NUM_OUTS) wrapper_add_audio_output(instance, outname[i], &r->outbufs[i]);
  wrapper_add_cc(instance, CC_SIZE, "Size", "size", 64);
  FOR(i, NUM_STAGES) wrapper_add_cc(instance, CC_GAIN+i, gainname1[i], gainname2[i], 0);
}

void plugin_destroy(struct instance* instance) {
  struct reverb *r = instance->plugin;
  free(r->tank_buf);
  r->tank_buf = NULL;
  free(instance->plugin);
  instance->plugin = NULL;
}

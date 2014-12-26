#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include "wrapper.h"

const char* plugin_name = "Mid-Side Reverb 2";
const char* plugin_persistence_name = "mjack_ms_reverb2";
const unsigned plugin_ladspa_unique_id = 12;

#define NUM_INS 1
#define NUM_OUTS 2
#define NUM_STAGES 8

#define CC_K 80

struct reverb {
  float* inbufs[NUM_INS];
  float* outbufs[NUM_OUTS];
  float *allpass_buf[NUM_OUTS][NUM_STAGES];
  int allpass_len[NUM_OUTS][NUM_STAGES];
  int allpass_pos[NUM_OUTS][NUM_STAGES];
  float allpass_time[NUM_OUTS][NUM_STAGES];
  double sr;
};

static void init(struct reverb* r, double nframes_per_second) {
  static double time[NUM_OUTS][NUM_STAGES] = {
    { 0.0112998, 0.02892348, 0.03295123, 0.0485128, 0.051856, 0.0669123, 0.0712357, 0.0881253 },
    { 0.0138915, 0.02123515, 0.03961282, 0.0451823, 0.055823, 0.0631522, 0.0752381, 0.0871258 },
  };
  FOR(o, NUM_OUTS) {
    FOR(s, NUM_STAGES) {
      r->allpass_time[o][s] = time[o][s];
      r->allpass_len[o][s] = (int) (r->allpass_time[o][s] * nframes_per_second + 0.5);
      r->allpass_buf[o][s] = calloc(1, sizeof(float) * r->allpass_len[o][s]); // TODO contiguous alloc
    }
  }
}

static void destroy(struct reverb* r) {
  FOR(o, NUM_OUTS) {
    FOR(s, NUM_STAGES) {
      free(r->allpass_buf[o][s]);
      r->allpass_buf[o][s] = NULL;
    }
  }
}

void plugin_process(struct instance* instance, int nframes) {
  struct reverb* r = instance->plugin;

  const float *in = r->inbufs[0];
  FOR(o, NUM_OUTS) {
    float *out = r->outbufs[o];
    if (in != out) {
      FOR(i, nframes) {
	out[i] = in[i];
      }
    }
  }
  float kshape = instance->wrapper_cc[CC_K] / 128.0;
  FOR(o, NUM_OUTS) {
    float *out = r->outbufs[o];
    FOR(i, NUM_STAGES) {
      float *buf = r->allpass_buf[o][i];
      int len = r->allpass_len[o][i];
      int pos = r->allpass_pos[o][i];
      //float k = exp(-r->allpass_time[i] * kshape * 16);
      //float k = kshape;
      //float k = exp(-log(r->allpass_time[i])*(1-kshape));
      float k = fmaxf(0.0f, 1.0f - (1.0f - kshape) / r->allpass_time[o][i]);
      FOR(j, nframes) {
	float a = out[j];
	float b = buf[pos];
	a -= b * k;
	b += a * k;
	out[j] = b;
	buf[pos] = a;
	pos+= 1;
	if (pos >= len) { pos = 0; }
      }
      r->allpass_pos[o][i] = pos;
    }
  }
  FOR(i, nframes) {
    double a = r->outbufs[0][i];
    double b = r->outbufs[1][i];
    r->outbufs[0][i] = 0.5 * (a + b);
    r->outbufs[1][i] = 0.5 * (a - b);
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  printf("plugin_init\n");
  struct reverb* r = memalign(4096, sizeof(struct reverb));
  memset(r, 0, sizeof(struct reverb));
  instance->plugin = r;
  printf("init\n");
  init(r, sample_rate);
#define MAX_NAME_LENGTH 16
  static char inname[NUM_INS][MAX_NAME_LENGTH];
  static char outname[NUM_OUTS][MAX_NAME_LENGTH];
  FOR(i, NUM_INS) snprintf(inname[i], MAX_NAME_LENGTH, "in %i", i);
  FOR(i, NUM_OUTS) snprintf(outname[i], MAX_NAME_LENGTH, "out %i", i);
  printf("audio ports\n");
  FOR(i, NUM_INS) wrapper_add_audio_input(instance, inname[i], &r->inbufs[i]);
  FOR(i, NUM_OUTS) wrapper_add_audio_output(instance, outname[i], &r->outbufs[i]);
  wrapper_add_cc(instance, CC_K, "K", "k", 64);
  printf("CCs\n");
  printf("exit plugin_init\n");
}

void plugin_destroy(struct instance* instance) {
  struct reverb* r = instance->plugin;
  destroy(r);
  free(instance->plugin);
  instance->plugin = NULL;
}

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Mid-Side Reverb 3";
const char* plugin_persistence_name = "mjack_ms_reverb3";
const unsigned plugin_ladspa_unique_id = 13;

#define NUM_INS 1
#define NUM_OUTS 2
#define NUM_STAGES 8

#define CC_SHAPE 80
#define CC_ALLPASS_TIME_START 81

#define MAX_STAGE_TIME_SECONDS 0.1

struct reverb {
  float* inbufs[NUM_INS];
  float* outbufs[NUM_OUTS];
  int allpasspos[NUM_OUTS][NUM_STAGES];
  float nframes_per_second;
  float *memory_pool_start;
  float *memory_pool_end;
  double sr;
};

static int gcd(int a, int b) {
  if (b == 0) { return a; }
  else return gcd(b, a % b);
}

static void init(struct reverb* r, double nframes_per_second) {
  r->nframes_per_second = nframes_per_second;
  int n = (int) (nframes_per_second * NUM_OUTS * NUM_STAGES * MAX_STAGE_TIME_SECONDS * 1.1);
  r->memory_pool_start = calloc(1, sizeof(float) * n);
  r->memory_pool_end = r->memory_pool_start + n;
}

static void destroy(struct reverb* r) {
  free(r->memory_pool_start);
  r->memory_pool_start = NULL;
  r->memory_pool_end = NULL;
}

void plugin_process(struct instance* instance, int nframes) {
  struct reverb* r = instance->plugin;
  float allpasstime[NUM_OUTS][NUM_STAGES];
  FOR(o, NUM_OUTS) {
    FOR(s, NUM_STAGES) {
      allpasstime[o][s] = MAX_STAGE_TIME_SECONDS * (instance->wrapper_cc[CC_ALLPASS_TIME_START + o * NUM_STAGES + s] + 1) / 128.0;
    }
  }
  float *allpassbuf[NUM_OUTS][NUM_STAGES];
  int allpasslen[NUM_OUTS][NUM_STAGES];
  float *alloc_ptr = r->memory_pool_start;
  FOR(o, NUM_OUTS) {
    FOR(s, NUM_STAGES) {
      int len = (int) (allpasstime[o][s] * r->nframes_per_second + 0.5f);
    try_again:
      FOR(t, s) {
	if (gcd(len, allpasslen[o][t]) != 1) {
	  len++;
	  goto try_again;
	}
      }
      //printf("%i %i %f %i\n", o, s, allpasstime[o][s], len);
      allpasslen[o][s] = len;
      allpassbuf[o][s] = alloc_ptr;
      alloc_ptr += len;
    }
  }

  const float *in = r->inbufs[0];
  FOR(o, NUM_OUTS) {
    float *out = r->outbufs[o];
    if (in != out) {
      FOR(i, nframes) {
	out[i] = in[i];
      }
    }
  }
  if (alloc_ptr >= r->memory_pool_end) {
    //printf("%i\n", (int) (r->memory_pool_end - r->memory_pool_start));
    printf("error: alloc_ptr >= r->memory_pool_end");
  }
  float kshape = (0.5f + instance->wrapper_cc[CC_SHAPE]) / 128.0f;
  FOR(o, NUM_OUTS) {
    float *out = r->outbufs[o];
    FOR(i, NUM_STAGES) {
      float *buf = allpassbuf[o][i];
      int len = allpasslen[o][i];
      int pos = r->allpasspos[o][i];

      // same tail decay rate:
      // tail coloration: strong
      // good for reverse reverb?
      // float k = exp(-allpasstime[o][i] * kshape * 16);

      // direct control: least colored tail?
      // tail coloration: weak
      //float k = kshape;

      // constant tail bandwidth
      float k = fmax(0.0f, 1.0f - kshape * kshape / allpasstime[o][i]);

      // same power from all tails
      // tail coloration: weak
      // flutter: medium
      //float k = fmax(0.0f, 1.0f - 4*kshape * kshape * sqrtf(allpasstime[o][i]));

      //float k = fmax(0.0f, 1.0f - kshape * kshape / sqrtf(allpasstime[o][i]));

      //float tanw = kshape * kshape / allpasstime[o][i]; // linear approx
      //float k = fmaxf(0.0f, (1 - tanw) / (1 + tanw));

      //printf("%i %i %f\n", o, i, k);

      FOR(j, nframes) {
	float a = out[j];
	float b = buf[pos];
	a += b * k;
	b -= a * k;
	out[j] = b;
	buf[pos] = a;
	pos+= 1;
	if (pos >= len) { pos = 0; }
      }
      r->allpasspos[o][i] = pos;
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
  printf("CCs\n");
  wrapper_add_cc(instance, CC_SHAPE, "K", "k", 64);
  static const char *outputname[NUM_OUTS] = { "mid", "side" };
  static char allpasstimename[NUM_OUTS][NUM_STAGES][MAX_NAME_LENGTH];
  FOR(o, NUM_OUTS) FOR(s, NUM_STAGES) snprintf(allpasstimename[o][s], MAX_NAME_LENGTH, "%s time %i", outputname[o], s);
  FOR(o, NUM_OUTS) FOR(s, NUM_STAGES) wrapper_add_cc(instance, CC_ALLPASS_TIME_START + o * NUM_STAGES + s, allpasstimename[o][s], allpasstimename[o][s], 64);
  printf("exit plugin_init\n");
}

void plugin_destroy(struct instance* instance) {
  struct reverb* r = instance->plugin;
  destroy(r);
  free(instance->plugin);
  instance->plugin = NULL;
}

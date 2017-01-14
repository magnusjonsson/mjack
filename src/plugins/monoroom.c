#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include "../wrappers/wrapper.h"

// Mono parallel comb reverb designed for natural f^2 mode density.
//
// TODO: decay time knob
// TODO: predelay time knob
// TODO: HF damping
// TODO: LF damping

const char* plugin_name = "Mono Room";
const char* plugin_persistence_name = "mjack_monoroom";
const unsigned plugin_ladspa_unique_id = 19;

#define KNOBS \
  X(CC_RT, 81, "Reverb Time")	  \
  X(CC_F0, 82, "Lowest Freq")	  \
  X(CC_FD, 83, "Delta Freq")	  \
  X(CC_P0, 84, "Lowest Predelay") \
  X(CC_PD, 85, "Delta Predelay")  \
  X(CC_DAMPING, 86, "Damping")	  \

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};


#define NUM_DELAYS 64
#define MAX_DELAY 16384
#define MASK (MAX_DELAY - 1)
#define BATCH_SIZE 32

#define MAX_PREDELAY (65536 * 16)
#define PREDELAY_MASK (MAX_PREDELAY - 1)

struct reverb {
  float* inbuf;
  float* outbuf;
  char cc[128];
  double nframes_per_second;
  int32_t predelay_pos;
  int32_t pos;
  struct {
    int32_t predelay_len;
    int32_t len; 
    float ingain;
    float fbgain_lpcoeff;
    float lpcoeff_mirror;
    float lpstate;
    float buf[MAX_DELAY];
    float freq_randoms;
    float predelay_randoms;
 } delay[NUM_DELAYS];
  float predelay_buf[MAX_PREDELAY] __attribute__((aligned(16)));
}  __attribute__((aligned(16)));

static void recompute(struct reverb *r) {
  float rt = 1.5 * pow(10.0, r->cc[CC_RT]/64.0 - 1.0);
  float base_freq = 20 * pow(10.0, r->cc[CC_F0]/64.0 - 1.0);
  float delta_freq = 4 * pow(10.0, r->cc[CC_FD]/64.0 - 1.0);
  float base_predelay = 0.05 * pow(10.0, r->cc[CC_P0]/64.0 - 1.0);
  float delta_predelay = 0.01 * pow(10.0, r->cc[CC_PD]/64.0 - 1.0);
  float sqrt_damping = 100.0 * pow(r->cc[CC_DAMPING]/127.0, 4.0);
  FOR(i, NUM_DELAYS) {
    float freq = base_freq + delta_freq * ((NUM_DELAYS - 1 - i) + r->delay[i].freq_randoms);
    float predelay = base_predelay + delta_predelay * (i + r->delay[i].predelay_randoms);
    float seconds = 1 / freq;
    float sqrt_seconds = sqrtf(seconds);
    float fbgain = pow(0.001, seconds / rt);
    float ingain = sqrt_seconds * pow(0.001, predelay / rt);
    float lpcoeff = 1 / (1 + sqrt_damping * sqrt_seconds);
    r->delay[i].len = r->nframes_per_second * seconds;
    r->delay[i].ingain = ingain;
    r->delay[i].fbgain_lpcoeff = fbgain * lpcoeff;
    r->delay[i].lpcoeff_mirror = 1 - lpcoeff;
    r->delay[i].predelay_len = r->nframes_per_second * predelay;
  }
}

static void init(struct reverb* r, double nframes_per_second) {
  FOR(i, NUM_DELAYS) {
    r->delay[i].freq_randoms = rand() / (RAND_MAX + 1.0);
    r->delay[i].predelay_randoms = rand() / (RAND_MAX + 1.0);
  }
  r->nframes_per_second = nframes_per_second;
  recompute(r);
}

static inline void minf(int *x, int y) {
  if (y < *x) {
    *x = y;
  }
}

void plugin_process(struct instance *instance, int nframes) {
  struct reverb *r = instance->plugin;
  FOR(i, 128) {
    if (r->cc[i] != instance->wrapper_cc[i]) {
      FOR(j, 128) { r->cc[j] = instance->wrapper_cc[j]; }
      recompute(r);
      break;
    }
  }

  // TODO break computation up into blocks of known max size to not
  // overflow predelay buffer

  // Copy input into predelay buffer
  const float *inbuf = r->inbuf;
  float *predelay_buf = r->predelay_buf;
  int32_t pos = r->pos;
  FOR(i, nframes) {
    predelay_buf[(pos + i) & PREDELAY_MASK] = inbuf[i];
  }

  // Clear output
  float *outbuf = r->outbuf;
  FOR(i, nframes) {
    outbuf[i] = 0;
  }

  // Add delays
  FOR(j, NUM_DELAYS) {
    int i = 0;
    float lpstate = r->delay[j].lpstate;
    float fbgain_lpcoeff = r->delay[j].fbgain_lpcoeff;
    float lpcoeff_mirror = r->delay[j].lpcoeff_mirror;
    float lpcoeff_mirror_2 = lpcoeff_mirror * lpcoeff_mirror;
    float fbgain_lpcoeff_lpcoeff_mirror = fbgain_lpcoeff * lpcoeff_mirror;
    float ingain = r->delay[j].ingain;
    int predelay_len = r->delay[j].predelay_len;
    int len = r->delay[j].len;
    while (i < nframes) {
      int n = nframes - i;
      int pos1 = (pos + i - predelay_len) & PREDELAY_MASK;
      int pos2 = (pos + i) & MASK;
      int pos3 = (pos + i - len) & MASK;
      minf(&n, MAX_PREDELAY - pos1);
      minf(&n, MAX_DELAY - pos2);
      minf(&n, MAX_DELAY - pos3);
      float const *pin = &r->predelay_buf[pos1];
      float       *pfi = &r->delay[j].buf[pos2];
      float       *pfb = &r->delay[j].buf[pos3];
      float       *pout = &outbuf[i];
      for(int k = 0; k < n; k++) {
	lpstate = lpstate * lpcoeff_mirror + pfb[k] * fbgain_lpcoeff;
	pfi[k] = lpstate + pin[k] * ingain;
	pout[k] += lpstate;
      }
      i += n;
    }
    r->delay[j].lpstate = lpstate;
  }
  r->pos = pos + nframes;
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct reverb* r = memalign(4096, sizeof(struct reverb));
  memset(r, 0, sizeof(struct reverb));
  instance->plugin = r;
  wrapper_add_audio_input(instance, "in", &r->inbuf);
  wrapper_add_audio_output(instance, "out", &r->outbuf);
#define X(name, value, label) wrapper_add_cc(instance, value, label, #name, 64);
  KNOBS
#undef X
  init(r, sample_rate);
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  instance->plugin = NULL;
}

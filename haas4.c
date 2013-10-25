#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <malloc.h>
#include "wrapper.h"

const char* plugin_name = "Haas4";
const char* plugin_persistence_name = "mjack_haas4";

#define NUM_INS 4
#define NUM_OUTS 2
#define MAX_NAME_LENGTH 16

static double gain[NUM_OUTS][NUM_INS] = {
  { 1.0, 0.9, 0.8, 0.7 },
  { 0.7, 0.8, 0.9, 1.0 }
};
static double offset_millis[NUM_OUTS][NUM_INS] = {
  { 0, 0, 2, 5 },
  { 5, 2, 0, 0 }
};
static char inname[NUM_INS][MAX_NAME_LENGTH];
static char outname[NUM_INS][MAX_NAME_LENGTH];

struct haas4 {
  int offset[NUM_OUTS][NUM_INS];
  float* inbufs[NUM_INS];
  float* outbufs[NUM_OUTS];
#define MIXBUF_LENGTH (1 << 16)
#define MIXBUF_MASK (MIXBUF_LENGTH - 1)
  int head;
  float mixbufs[NUM_OUTS][MIXBUF_LENGTH];
};

static void init(struct haas4* h, double sample_rate) {
  FOR(out, NUM_OUTS) FOR(in, NUM_INS) h->offset[out][in] = (int) (offset_millis[out][in] * sample_rate / 1000.0 + 0.5);
  FOR(out, NUM_OUTS) FOR(in, NUM_INS) fprintf(stderr, "in %i, out %i, offset %i\n", in, out, h->offset[out][in]);
}

static void mix(struct haas4* h, int offset, int nframes) {
  FOR(out, NUM_OUTS) FOR(in, NUM_INS) FOR(i, nframes) h->mixbufs[out][(h->head+i+h->offset[out][in]) & MIXBUF_MASK] += h->inbufs[in][offset+i] * gain[out][in];
  FOR(out, NUM_OUTS) FOR(i, nframes) h->outbufs[out][offset+i] = h->mixbufs[out][(h->head+i) & MIXBUF_MASK];
  FOR(out, NUM_OUTS) FOR(i, nframes) h->mixbufs[out][(h->head+i) & MIXBUF_MASK] = 0;
  h->head += nframes;
}

void plugin_process(struct instance* instance, int nframes) {
  struct haas4 *h = instance->plugin;
  int offset = 0;
  while (nframes > MIXBUF_LENGTH) { mix(h, offset, MIXBUF_LENGTH); offset += MIXBUF_LENGTH; nframes -= MIXBUF_LENGTH; }
  if (nframes > 0) mix(h, offset, nframes);
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct haas4 *h = calloc(1, sizeof(struct haas4));
  instance->plugin = h;
  init(h, sample_rate);
  FOR(i, NUM_INS) snprintf(inname[i], MAX_NAME_LENGTH, "in %i", i);
  FOR(i, NUM_OUTS) snprintf(outname[i], MAX_NAME_LENGTH, "out %i", i);
  // TODO what does JackPortIsTerminal mean?
  FOR(i, NUM_INS) wrapper_add_audio_input(instance, inname[i], &h->inbufs[i]);
  FOR(i, NUM_OUTS) wrapper_add_audio_output(instance, outname[i], &h->outbufs[i]);
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  free(instance);
}

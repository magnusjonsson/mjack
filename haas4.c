#include <stdio.h>
#include <limits.h>
#include <unistd.h>

#include "wrapper.h"

#define NUM_INS 4
#define NUM_OUTS 2
#define MAX_NAME_LENGTH 16
#define FOR(var,limit) for(int var = 0; var < limit; ++var)

static double gain[NUM_OUTS][NUM_INS] = {
  { 1.0, 0.9, 0.8, 0.7 },
  { 0.7, 0.8, 0.9, 1.0 }
};
static double offset_millis[NUM_OUTS][NUM_INS] = {
  { 0, 0, 2, 5 },
  { 5, 2, 0, 0 }
};
static int offset[NUM_OUTS][NUM_INS];

static char inname[NUM_INS][MAX_NAME_LENGTH];
static char outname[NUM_INS][MAX_NAME_LENGTH];
static float* inbufs[NUM_INS];
static float* outbufs[NUM_OUTS];
#define MIXBUF_LENGTH (1 << 16)
#define MIXBUF_MASK (MIXBUF_LENGTH - 1)
static int head;
static float mixbufs[NUM_OUTS][MIXBUF_LENGTH];

static void init(double sample_rate) {
  fprintf(stderr, "initializing, sample rate = %f\n", sample_rate);
  FOR(out, NUM_OUTS) FOR(in, NUM_INS) offset[out][in] = (int) (offset_millis[out][in] * sample_rate / 1000.0 + 0.5);
  FOR(out, NUM_OUTS) FOR(in, NUM_INS) fprintf(stderr, "in %i, out %i, offset %i\n", in, out, offset[out][in]);
}

static void mix(int nframes) {
  FOR(out, NUM_OUTS) FOR(in, NUM_INS) FOR(i, nframes) mixbufs[out][(head+i+offset[out][in]) & MIXBUF_MASK] += inbufs[in][i] * gain[out][in];
  FOR(out, NUM_OUTS) FOR(i, nframes) outbufs[out][i] = mixbufs[out][(head+i) & MIXBUF_MASK];
  FOR(out, NUM_OUTS) FOR(i, nframes) mixbufs[out][(head+i) & MIXBUF_MASK] = 0;
  head += nframes;
  FOR(in, NUM_OUTS) inbufs[in] += nframes;
  FOR(out, NUM_OUTS) outbufs[out] += nframes;
}

void plugin_process(int nframes) {
  while (nframes > MIXBUF_LENGTH) { mix(MIXBUF_LENGTH); nframes -= MIXBUF_LENGTH; }
  if (nframes > 0) mix(nframes);
}

int main(int argc, char** argv) {
  wrapper_init(&argc, &argv, "Haas4", "mjack_haas4");
  init(wrapper_get_sample_rate());
  FOR(i, NUM_INS) snprintf(inname[i], MAX_NAME_LENGTH, "in %i", i);
  FOR(i, NUM_OUTS) snprintf(outname[i], MAX_NAME_LENGTH, "out %i", i);
  // TODO what does JackPortIsTerminal mean?
  FOR(i, NUM_INS) wrapper_add_audio_input(inname[i], &inbufs[i]);
  FOR(i, NUM_OUTS) wrapper_add_audio_output(outname[i], &outbufs[i]);
  wrapper_run();
  return 0;
}

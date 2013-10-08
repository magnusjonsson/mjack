#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include "wrapper.h"

#define NUM_INS 2
#define NUM_OUTS 2

#define NUM_STAGES 4
#define MIX_SIZE 4

#define CC_WET_LEVEL 91
#define CC_FEEDBACK 72
#define CC_DECAY 80

static float* inbufs[NUM_INS];
static float* outbufs[NUM_OUTS];

static double dt;
static float* tank_buf;
static int tank_len;
static int inoffs[NUM_INS];
static int outoffs[NUM_OUTS];
static int mixoffs[NUM_STAGES][MIX_SIZE];
static float decaygain[NUM_STAGES][MIX_SIZE];
static int base;
#define SQRT_ONE_HALF 0.707106781

static double frand(void) {
  return rand() / (RAND_MAX + 1.0);
}

static int irand(int max_plus_1) {
  int i = (int) (frand() * max_plus_1);
  return i;
}

static void init_mixoffs() {
  inoffs[0] = 0;
  inoffs[1] = tank_len / 4 * 2;
  outoffs[0] = tank_len / 4 * 2 - 1;
  outoffs[1] = tank_len / 4 * 4 - 1;

  FOR(s, NUM_STAGES) {
    FOR(j, MIX_SIZE) {
      mixoffs[s][j] = tank_len/4 * (s & 3) + tank_len/4/MIX_SIZE*j + irand(tank_len/4/MIX_SIZE - 1);
      for (int k = 0; k < j; ++k) {
        if (mixoffs[s][j] >= mixoffs[s][k]) { // TODO prove this algo...
          ++mixoffs[s][j];
        }
      }
    }
  }
}

static int delay_length(int offs) {
  int min_distance = tank_len;
  FOR(s, NUM_STAGES) {
    FOR(j, MIX_SIZE) {
      int distance = offs - mixoffs[s][j];
      if (distance <= 0) distance += tank_len;
      if (distance < min_distance) min_distance = distance;
    }
  }
  return min_distance;
}
static void init_decaygain() {
  FOR(s, NUM_STAGES) {
    FOR(j, MIX_SIZE) {
      int len = delay_length(mixoffs[s][j]);
      decaygain[s][j] = pow(0.001, dt * len / 6.0);
    }
  }
}
static void init(jack_nframes_t nframes_per_second) {
  dt = 1.0 / nframes_per_second;
  tank_len = (int)(nframes_per_second * 0.5);
  tank_buf = calloc(tank_len, sizeof(float));
  init_mixoffs();
  init_decaygain();
}

static int tank_index(int offs) {
  CHECK(base >= 0 && base < tank_len, "base");
  CHECK(offs >= 0 && offs < tank_len, "offs");
  int index = base - offs;
  if (index < 0) index = index + tank_len;
  CHECK(index >= 0 && index < tank_len, "index");
  return index;
}

static void mix2(int s) {
  int i0 = tank_index(mixoffs[s][0]);
  int i1 = tank_index(mixoffs[s][1]);
  double a = (tank_buf[i0] - tank_buf[i1]) * SQRT_ONE_HALF;
  double b = (tank_buf[i0] + tank_buf[i1]) * SQRT_ONE_HALF;
  tank_buf[i0] = a;
  tank_buf[i1] = b;
}

static void mix4(int s) {
  int i0 = tank_index(mixoffs[s][0]);
  int i1 = tank_index(mixoffs[s][1]);
  int i2 = tank_index(mixoffs[s][2]);
  int i3 = tank_index(mixoffs[s][3]);
  /*
  double g = cc[CC_DECAY] / 127.0;
  double t0 = tank_buf[i0] * g;
  double t1 = tank_buf[i1] * g;
  double t2 = tank_buf[i2] * g;
  double t3 = tank_buf[i3] * g;
  */
  float t0 = tank_buf[i0] * decaygain[s][0];
  float t1 = tank_buf[i1] * decaygain[s][1];
  float t2 = tank_buf[i2] * decaygain[s][2];
  float t3 = tank_buf[i3] * decaygain[s][3];
  double r = (t0 + t1 + t2 + t3) * 0.5;
  tank_buf[i0] = t0 - r;
  tank_buf[i1] = t1 - r;
  tank_buf[i2] = t2 - r;
  tank_buf[i3] = t3 - r;
}

static void mix8(int s) {
  int i0 = tank_index(mixoffs[s][0]);
  int i1 = tank_index(mixoffs[s][1]);
  int i2 = tank_index(mixoffs[s][2]);
  int i3 = tank_index(mixoffs[s][3]);
  int i4 = tank_index(mixoffs[s][4]);
  int i5 = tank_index(mixoffs[s][5]);
  int i6 = tank_index(mixoffs[s][6]);
  int i7 = tank_index(mixoffs[s][7]);
  double g = cc[CC_DECAY] / 127.0;
  double t0 = tank_buf[i0] * g;
  double t1 = tank_buf[i1] * g;
  double t2 = tank_buf[i2] * g;
  double t3 = tank_buf[i3] * g;
  double t4 = tank_buf[i4] * g;
  double t5 = tank_buf[i5] * g;
  double t6 = tank_buf[i6] * g;
  double t7 = tank_buf[i7] * g;
  /*
  double t0 = tank_buf[i0] * decaygain[s][0];
  double t1 = tank_buf[i1] * decaygain[s][1];
  double t2 = tank_buf[i2] * decaygain[s][2];
  double t3 = tank_buf[i3] * decaygain[s][3];
  double t4 = tank_buf[i4] * decaygain[s][4];
  double t5 = tank_buf[i5] * decaygain[s][5];
  double t6 = tank_buf[i6] * decaygain[s][6];
  double t7 = tank_buf[i7] * decaygain[s][7];
  */
  double r = (t0 + t1 + t2 + t3 + t4 + t5 + t6 + t7) * (2.0 / 8.0);
  tank_buf[i0] = t0 - r;
  tank_buf[i1] = t1 - r;
  tank_buf[i2] = t2 - r;
  tank_buf[i3] = t3 - r;
  tank_buf[i4] = t4 - r;
  tank_buf[i5] = t5 - r;
  tank_buf[i6] = t6 - r;
  tank_buf[i7] = t7 - r;
}
static double square(double x) {
  return x * x;
}

static void process(int nframes) {
  double reverb_gain = square(cc[CC_WET_LEVEL] * (2.0 / 127.0));
  double feedback_gain = square(cc[CC_FEEDBACK] / 127.0);
  FOR(f, nframes) {
    FOR(o, NUM_OUTS) {
      int index = tank_index(outoffs[o]);
      outbufs[o][f] = reverb_gain * tank_buf[index];
      tank_buf[index] *= feedback_gain;
    }
    FOR(i, NUM_INS) {
      tank_buf[tank_index(inoffs[i])] += inbufs[i][f];
    }
    if (MIX_SIZE == 2) FOR(s, NUM_STAGES) mix2(s);
    if (MIX_SIZE == 4) FOR(s, NUM_STAGES) mix4(s);
    if (MIX_SIZE == 8) FOR(s, NUM_STAGES) mix8(s);
    ++base;
    if (base >= tank_len) base = 0;
  }
}

int main(int argc, char** argv) {
  wrapper_init(&argc, &argv, "Reverb", "mjack_reverb");
  srand(1053);
  init(wrapper_get_sample_rate());
#define MAX_NAME_LENGTH 16
  static char inname[NUM_INS][MAX_NAME_LENGTH];
  static char outname[NUM_INS][MAX_NAME_LENGTH];
  FOR(i, NUM_INS) snprintf(inname[i], MAX_NAME_LENGTH, "in %i", i);
  FOR(i, NUM_OUTS) snprintf(outname[i], MAX_NAME_LENGTH, "out %i", i);
  FOR(i, NUM_INS) wrapper_add_audio_input(inname[i], &inbufs[i]);
  FOR(i, NUM_OUTS) wrapper_add_audio_output(outname[i], &outbufs[i]);
  wrapper_add_cc(CC_WET_LEVEL, "Wet", "wet", 64);
  wrapper_add_cc(CC_FEEDBACK, "Feedback", "feedback", 64);
  wrapper_add_cc(CC_DECAY, "Decay", "decay", 64);
  wrapper_run();
  return 0;
}

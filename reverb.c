#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "wrapper.h"

#define NUM_INS 2
#define NUM_OUTS 2

#define NUM_STAGES 4
#define MIX_SIZE 4

#define CC_WET_LEVEL 91
#define CC_FEEDBACK 72
#define CC_DECAY 80

#define BUF_LEN 32
static float* inbufs[NUM_INS];
static float* outbufs[NUM_OUTS];
static float buf[NUM_STAGES][MIX_SIZE][BUF_LEN];
static float buf_offs[NUM_STAGES][MIX_SIZE];
static double dt;
static float* tank_buf;
static int tank_len;
static float decay_gain[NUM_STAGES][MIX_SIZE];
static int base;
#define SQRT_ONE_HALF 0.707106781

static double frand(void) {
  return rand() / (RAND_MAX + 1.0);
}

static int irand(int max_plus_1) {
  int i = (int) (frand() * max_plus_1);
  return i;
}

static void init_buf_offs() {
  FOR(s, NUM_STAGES) {
    FOR(m, MIX_SIZE) {
      buf_offs[s][m] = tank_len/NUM_STAGES * s + tank_len/NUM_STAGES/MIX_SIZE*m + irand(tank_len/NUM_STAGES/MIX_SIZE - BUF_LEN);
    }
  }
}

static int delay_length(int offs) {
  int min_distance = tank_len;
  FOR(s, NUM_STAGES) {
    FOR(m, MIX_SIZE) {
      int distance = offs - buf_offs[s][m];
      if (distance <= 0) distance += tank_len;
      if (distance < min_distance) min_distance = distance;
    }
  }
  return min_distance;
}
static void init_decay_gain() {
  FOR(s, NUM_STAGES) {
    FOR(m, MIX_SIZE) {
      int len = delay_length(buf_offs[s][m]);
      decay_gain[s][m] = pow(0.001, dt * len / 2.5);
    }
  }
}
static void init(double nframes_per_second) {
  dt = 1.0 / nframes_per_second;
  tank_len = (int)(nframes_per_second * 1.0);
  tank_buf = calloc(tank_len, sizeof(float));
  init_buf_offs();
  init_decay_gain();
}

static void mix2(int s, int n) {
  FOR(i, n) {
    float a = buf[s][0][i];
    float b = buf[s][1][i];
    buf[s][0][i] = (a + b) * SQRT_ONE_HALF;
    buf[s][1][i] = (b - a) * SQRT_ONE_HALF;
  }
}

static void mix4(int s, int n) {
  FOR(i, n) {
    float t0 = buf[s][0][i] * decay_gain[s][0];
    float t1 = buf[s][1][i] * decay_gain[s][1];
    float t2 = buf[s][2][i] * decay_gain[s][2];
    float t3 = buf[s][3][i] * decay_gain[s][3];
    double r = (t0 + t1 + t2 + t3) * 0.5;
    buf[s][0][i] = t0 - r;
    buf[s][1][i] = t1 - r;
    buf[s][2][i] = t2 - r;
    buf[s][3][i] = t3 - r;
  }
}

static void mix8(int s, int n) {
  FOR(i, n) {
    double t0 = buf[s][0][i];
    double t1 = buf[s][1][i];
    double t2 = buf[s][2][i];
    double t3 = buf[s][3][i];
    double t4 = buf[s][4][i];
    double t5 = buf[s][5][i];
    double t6 = buf[s][6][i];
    double t7 = buf[s][7][i];
    double r = (t0 + t1 + t2 + t3 + t4 + t5 + t6 + t7) * (2.0 / 8.0);
    buf[s][0][i] = t0 - r;
    buf[s][1][i] = t1 - r;
    buf[s][2][i] = t2 - r;
    buf[s][3][i] = t3 - r;
    buf[s][4][i] = t4 - r;
    buf[s][5][i] = t5 - r;
    buf[s][6][i] = t6 - r;
    buf[s][7][i] = t7 - r;
  }
}
static double square(double x) {
  return x * x;
}

void plugin_process(int nframes) {
  double reverb_gain = square(wrapper_cc[CC_WET_LEVEL] * (2.0 / 127.0));
  double feedback_gain = -square(wrapper_cc[CC_FEEDBACK] / 127.0);
  int io_base = 0;
  while (nframes > 0) {
    int n = nframes;
    if (n > BUF_LEN) n = BUF_LEN;
    FOR(s, NUM_STAGES) {
      FOR(m, MIX_SIZE) {
	int j = base - buf_offs[s][m];
	if (j < 0) {
	  j += tank_len;
	}
	if (j + BUF_LEN <= tank_len) {
	  memcpy(buf[s][m], tank_buf + j, n * sizeof(float));
	}
	else {
	  memcpy(buf[s][m], tank_buf + j, (tank_len - j) * sizeof(float));
	  memcpy(buf[s][m] + tank_len - j, tank_buf, (j + BUF_LEN - tank_len) * sizeof(float));
	}
      }
    }
    FOR(o, NUM_OUTS) {
      FOR(i, n) {
	outbufs[o^1][io_base + i] = reverb_gain * buf[NUM_STAGES/2*o][0][i];
	buf[NUM_STAGES/2*o][0][i] *= feedback_gain;
	buf[NUM_STAGES/2*o][0][i] += inbufs[o][io_base + i];
      }
    }
    if (MIX_SIZE == 2) FOR(s, NUM_STAGES) mix2(s, BUF_LEN);
    if (MIX_SIZE == 4) FOR(s, NUM_STAGES) mix4(s, BUF_LEN);
    if (MIX_SIZE == 8) FOR(s, NUM_STAGES) mix8(s, BUF_LEN);
    FOR(s, NUM_STAGES) {
      FOR(m, MIX_SIZE) {
	int j = base - buf_offs[s][m];
	if (j < 0) {
	  j += tank_len;
	}
	if (j + BUF_LEN <= tank_len) {
	  memcpy(tank_buf + j, buf[s][m], n * sizeof(float));
	}
	else {
	  memcpy(tank_buf + j, buf[s][m], (tank_len - j) * sizeof(float));
	  memcpy(tank_buf, buf[s][m] + tank_len - j, (j + BUF_LEN - tank_len) * sizeof(float));
	}
      }
    }
    base += n; if (base >= tank_len) base -= tank_len;
    io_base += n;
    nframes -= n;
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

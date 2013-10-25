#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <math.h>
#include <malloc.h>
#include "wrapper.h"

const char* plugin_name = "APChain";
const char* plugin_persistence_name = "mjack_apchain";

#define NUM_STAGES 16
#define FOR(var,limit) for(int var = 0; var < limit; ++var)
#define CC_CUTOFF 77

struct apchain {
  float* inbuf;
  float* outbuf;

  double dt;

  double apstate[NUM_STAGES];
};

static void init(struct apchain* a, double nframes_per_second) {
  a->dt = 1.0 / nframes_per_second;
}

static double calc_apcoeff(double freq) {
    /*

      H(s) = (1 - s/w) / (1 + s/w)

      BLT: s = 2 (1 - z^-1) / (1 + z^-1)
             = 2 (z - 1) / (z + 1)

      H(z) = (1 - 2/w (z-1) / (z+1)) / (1 + 2/w (z-1) / (z+1))
           = ((z+1) - 2/w (z-1)) / ((z+1) + 2/w (z-1))
           = ((1-2/w)z + (1+2/w)) / ((1+2/w)z + (1-2/w))
           = (az + 1) / (z + a)

      a = (1-2/w)/(1+2/w)
        = (w-2)/(w+2)


      w = 2 tan (w_digital / 2)

      (((
      a = (2 tan - 2) / (2 tan + 2)
        = (tan - 1) / (tan + 1)
        = (sin/cos - 1) / (sin + cos)
        = (sin - cos) / (sin + cos) 
      )))

               +--------(k)---+
               V              |
      X ----->(+)-+->[ z^-1 ]-+->(+)--> Y
                  |               A
                  +-----(-k)------+

     U = (X + kUz^-1)
     U (1 - kz^-1) = X
     U/X = 1 / (1 - kz^-1)

     Y = -kU + Uz^-1
     Y/U = (k - z^-1)

     Y/X = (k - z^-1) / (1 - kz^-1)
   */

  if (freq > 0.49) freq = 0.49;
  double w_digital = 2 * 3.141592 * freq;
  double w_analog = 2 * tan(w_digital / 2);
  double apcoeff = -(w_analog-2)/(w_analog+2);
  return apcoeff;
}

static double ap_tick(double* state, double in, double apcoeff) {
  double out = *state;
  in += apcoeff * out;
  out -= apcoeff * in;
  *state = in;
  return out;
}

void plugin_process(struct instance* instance, int nframes) {
  struct apchain *a = instance->plugin;
  double freq = 440.0 * pow(2.0, (instance->wrapper_cc[CC_CUTOFF] - 69 + 32) / 12.0);
  double apcoeff = calc_apcoeff(freq * a->dt);
  FOR(f, nframes) {
    double x = a->inbuf[f] + 1e-12;
    FOR(s, NUM_STAGES) {
      x = ap_tick(&a->apstate[s], x, apcoeff);
    }
    a->outbuf[f] = x;
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  struct apchain *a = calloc(1, sizeof(struct apchain));
  instance->plugin = a;
  init(a, sample_rate);
  wrapper_add_audio_input(instance, "in", &a->inbuf);
  wrapper_add_audio_output(instance, "out", &a->outbuf);
  wrapper_add_cc(instance, CC_CUTOFF, "Cutoff", "cutoff", 64);
}

void plugin_destroy(struct instance* instance) {
  free(instance->plugin);
  instance->plugin = NULL;
}

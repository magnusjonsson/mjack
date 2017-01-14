#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "SawSynth3";
const char* plugin_persistence_name = "mjack_sawsynth3";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif 

#define FOR(var,limit) for(int var = 0; var < limit; ++var)

static void* midi_in_buf;
static float* audio_out_buf;
static char key_is_pressed[128];
static int key_count_pressed;
static int current_key = -1;

// constants
static double dt;

// settings

#define CC_VOLUME 7
#define CC_CUTOFF 77
#define CC_RESONANCE 71
#define CC_TRACKING 80
#define CC_GRIT 75
#define CC_DETUNE 76

// dsp control values
static double gain;
static double osc_freq;
static double env_freq;

// dsp state
struct osc_state {
  double phase;
  double delayed_out;
};
static struct osc_state osc1_state;
static struct osc_state osc2_state;

struct svf_state {
  double last_input;
  double z1, z2;
};

static struct svf_state svf_state;
static double env_state;

#if 0
static double svf_tick(struct svf_state* state, double input, double freq, double q) {
  freq *= dt;
  if (freq > 0.499) {
    freq = 0.499;
  }
  double omega = 2 * M_PI * freq;
  /*                                                                          
    // compute output                                                          

    double hp = in - q * bp - lp;
    double bp = z1 + f * hp;
    double lp = z2 + f * bp;

    // update state

    z1 += 2 * f * hp;
    z2 += 2 * f * bp;

    // solve for hp

    hp = in - q * (z1 + f * hp) - (z2 + f * (z1 + f * hp))
       = in - q * z1  - q * f * hp - z2 - f * z1 - f * f * hp

    (1 + q * f + f * f) hp = in - (q + f) * z1 - z2;
  */

  double f = tan(0.5 * omega);
  double r = f + q;
  double g = 1 / (f * r + 1);

  // calculate outputs
  double hp = (input - r * state->z1 - state->z2) * g;
  double bp = state->z1 + f * hp;
  double lp = state->z2 + f * bp;

  // update state
  state->z1 = bp + f * hp;
  state->z2 = lp + f * bp;

  return lp;
}
#endif
static double shape(double x) {
  return fabs(x) < 1e-12 ? 1.0 : tanh(x) / x;
}

static double svf_tick_nonlinear(struct svf_state* state, double input, double freq, double q, double grit) {
  freq *= dt;
  if (freq > 0.499) {
    freq = 0.499;
  }
  double omega = 2 * M_PI * freq;
  /*                                                                          
    // compute output                                                          

    double hp = in - q * bp - lp;
    double bp = z1 + f * tanh(hp) = z1 + k1 * hp where k1 = f * tanh(hp)/hp
    double lp = z2 + f * tanh(lp) = z2 + k2 * lp where k2 = f * tanh(lp)/lp

    // update state

    z1 += 2 * k1 * hp;
    z2 += 2 * k2 * bp;

    // solve for hp

    hp = in - q * (z1 + k1 * hp) - (z2 + k2 * (z1 + k1 * hp))
       = in - q * z1  - q * k1 * hp - z2 - k2 * z1 - k1 * k2 * hp

    (1 + q * k1 + k1 * k2) hp = in - (q + k2) * z1 - z2;
  */

  double f = tan(0.5 * omega);

  grit *= grit;
  double half_delayed_input = (input + state->last_input) * 0.5;
  double fb = half_delayed_input - q * state->z1 - state->z2;
  double k1 = f * shape(fb * grit);
  double k2 = f * shape(state->z2 * grit);

  double r = q + k2;
  double g = 1 / (k1 * r + 1);

  // calculate outputs
  double hp = (input - r * state->z1 - state->z2) * g;
  double bp = state->z1 + k1 * hp;
  double lp = state->z2 + k2 * bp;

  // update state
  state->last_input = input;
  state->z1 = bp + k1 * hp;
  state->z2 = lp + k2 * bp;

  return lp;
}


static void init(double sample_rate) {
  dt = 1.0 / sample_rate;
  printf("dt = %lg\n", dt);
}
static void handle_midi_note_off(int key, int velocity) {
  if (key_is_pressed[key]) {
    key_is_pressed[key] = 0;
    --key_count_pressed;
    if (key_count_pressed == 0) {
      gain = 0.0;
    }
  }
}

static void handle_midi_note_on(int key, int velocity) {
  if (!key_is_pressed[key]) {
    key_is_pressed[key] = 1;
    ++key_count_pressed;
    current_key = key;
    osc_freq = 440.0 * pow(2.0, (key - 69) / 12.0);
    env_freq = osc_freq * 1.0;
    gain = velocity / 127.0;
  }
}

static void handle_midi_event(const struct midi_event* event) {
  switch (event->buffer[0] & 0xf0) {
  case 0x80:
    handle_midi_note_off(event->buffer[1], event->buffer[2]);
    break;
  case 0x90:
    if (event->buffer[2] == 0) {
      handle_midi_note_off(event->buffer[1], 64);
    } else {
      handle_midi_note_on(event->buffer[1], event->buffer[2]);
    }
    break;
  }
}

static double tick_osc_raw(struct osc_state* osc_state, double freq) {
  osc_state->phase += freq * dt;
  if (osc_state->phase > 0.5) { osc_state->phase -= 1.0; }
  return osc_state->phase;
}

static double tick_osc(struct osc_state* osc_state, double freq) {
  return tick_osc_raw(osc_state, freq);
}

static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  double svf_freq = 440.0 * pow(2.0, (current_key - 69 - 12) / 12.0 * (instance->wrapper_cc[CC_TRACKING] / 127.0) + (instance->wrapper_cc[CC_CUTOFF] - 69 + 12) / 12.0);
  double svf_q = 1.0 - instance->wrapper_cc[CC_RESONANCE] / 127.0;
  double svf_grit = (1 + instance->wrapper_cc[CC_GRIT]) * (2.0/127.0);
  double volume = instance->wrapper_cc[CC_VOLUME] * instance->wrapper_cc[CC_VOLUME] / (127.0 * 127.0) * 0.25;
  double detune = instance->wrapper_cc[CC_DETUNE] * 0.0078125 / 128.0;
  double osc1_freq = osc_freq * (1 - detune);
  double osc2_freq = osc_freq * (1 + detune);
  for (int i = start_frame; i < end_frame; ++i) {
    double osc1 = tick_osc(&osc1_state, osc1_freq);
    double osc2 = tick_osc(&osc2_state, osc2_freq);
    double osc = detune == 0 ? osc1 : (osc1 + osc2) * 0.707;
    double env = env_state += (gain - env_state) * 2 * 3.141592 * env_freq * dt;
    double svf = svf_tick_nonlinear(&svf_state, osc, svf_freq * (1+10*env), svf_q, svf_grit);
    audio_out_buf[i] = svf * env * volume;
  }
}

void plugin_process(struct instance* instance, int nframes) {
  int num_events = wrapper_get_num_midi_events(midi_in_buf);
  int sample_index = 0;
  FOR(e, num_events) {
    struct midi_event event = wrapper_get_midi_event(midi_in_buf, e);
    handle_midi_event(&event);
    if (sample_index < event.time) {
      generate_audio(instance, sample_index, event.time);
      sample_index = event.time;
    }
  }
  if (sample_index < nframes) {
    generate_audio(instance, sample_index, nframes);
  }
}

void plugin_init(struct instance* instance, double sample_rate) {
  init(sample_rate);
  wrapper_add_midi_input(instance, "midi in", &midi_in_buf);
  wrapper_add_audio_output(instance, "audio out", &audio_out_buf);
  wrapper_add_cc(instance, CC_VOLUME, "Volume", "volume", 64);
  wrapper_add_cc(instance, CC_CUTOFF, "Cutoff", "cutoff", 64);
  wrapper_add_cc(instance, CC_RESONANCE, "Resonance", "resonance", 64);
  wrapper_add_cc(instance, CC_GRIT, "Grit", "grit", 0);
  wrapper_add_cc(instance, CC_TRACKING, "Tracking", "tracking", 64);
  wrapper_add_cc(instance, CC_DETUNE, "Detune", "detune", 64);
}

void plugin_destroy(struct instance* instance) {
}

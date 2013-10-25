#include <jack/midiport.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

#include "wrapper.h"

const char* plugin_name = "SawSynth";
const char* plugin_persistence_name = "mjack_sawsynth";

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
#define CC_BRIGHTNESS 76 // TODO assign

// dsp control values
static double gain;
static double lfo1_freq;
static double lfo2_freq;
static double osc_freq;
static double env_freq;
static double env_q;

// dsp state
static double lfo1_phase;
static double lfo2_phase;
struct osc_state {
  double phase;
  double delayed_out;
};
static struct osc_state osc_state;
struct svf_state {
  double last_input;
  double z1, z2;
};

static struct svf_state svf_state;
static struct svf_state env_state;

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
  lfo1_freq = 5.0;
  lfo2_freq = 0.32195813;
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
    env_freq = osc_freq * 1;
    env_q = 1.0;
    gain = velocity / 127.0;
  }
}

static void handle_midi_event(const jack_midi_event_t* event) {
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

static double tick_lfo(double* lfo_phase, double lfo_freq) {
  *lfo_phase += lfo_freq * dt;
  if (*lfo_phase > 0.5) {
    *lfo_phase -= 1.0;
  }
  double lfo = *lfo_phase * 4;
  if (lfo > 1.0) {
    lfo = 2 - lfo;
  } else if (lfo < -1.0) {
    lfo = -2 - lfo;
  }
  return lfo;
}

/*
static double tick_osc_box(struct osc_state* osc_state, double freq) {
  double phase_inc = freq * dt;
  double curr_out = osc_state->phase + (1.0/2.0) * phase_inc;
  osc_state->phase += phase_inc;
  while (osc_state->phase > 0.5) {
    double t = (osc_state->phase - 0.5) / phase_inc;
    curr_out -= t;
    osc_state->phase -= 1.0;
  }
  return curr_out;
}
*/
#if 0
static double tick_osc_hat(struct osc_state* osc_state, double freq) {
  double phase_inc = freq * dt;
  /*
    A saw filtered by a hat filter with
    impulse response f(t):
    
    f(t) = t   (0 <= t <= 1)
           2-t (1 <= t <= 2)
           0   elsewhere

    Scribblings:
    
    integral[0..1] (a+bt) * (1 - t) dt =
    integral[0..1] (a + (b-a)t - b*t^2 dt =
    [0..1] (a*t + (b-a)*t^2/2 - b*t^3/3) =
    a + (b-a)/2 - b/3 =
    a/2 + b/6

    integral[0..1] (a+bt) * t dt =
    integral[0..1] at + bt^2 dt =
    [0..1] (a*t^2/2 + b*t^3/3 dt =
    a/2 + b/3

    integral[A..B] (1-t) dt =
    [A..B] (t - t^2/2)

    a(B-A) (b-a)(B^2-A^2)/2 - b(B^3 - A^3)/3

    integral[A..B] t dt =
    [A..B] (t^2/2) =
    a(B^2-A^2)/2
   */
  double next_out = (1.0/2.0) * osc_state->phase + (1.0/3.0) * phase_inc;
  double curr_out = (1.0/2.0) * osc_state->phase + (1.0/6.0) * phase_inc;
  osc_state->phase += phase_inc;
  while (osc_state->phase > 0.5) {
    double t = (osc_state->phase - 0.5) / phase_inc;
    curr_out -= (1.0/2.0) * (t*t);
    next_out -= t - (1.0/2.0) * (t*t);
    osc_state->phase -= 1.0;
  }
  curr_out += osc_state->delayed_out;
  osc_state->delayed_out = next_out;
  return curr_out;
}
#endif

static double tick_osc_ln(struct osc_state* osc_state, double freq, double brightness) {
  osc_state->phase += freq * dt;
  if (osc_state->phase > 0.5) { osc_state->phase -= 1.0; }
  
  double k = brightness;
  //static double lastk;
  //if (k != lastk) { printf("%lf\n", k); lastk = k; }
  double x = 1.0 + k * cos(osc_state->phase * 2 * 3.1415917);
  double y = k * sin(osc_state->phase * 2 * 3.1415917);

  //return y / sqrt(1 + x*x) / k;
  return atan2(y,x) / k;
}

static double tick_osc(struct osc_state* osc_state, double freq, double brightness) {
  return tick_osc_ln(osc_state, freq, brightness);
}

static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  for (int i = start_frame; i < end_frame; ++i) {
    double lfo1 = tick_lfo(&lfo1_phase, lfo1_freq);
    double lfo2 = tick_lfo(&lfo2_phase, lfo2_freq);
    double k = instance->wrapper_cc[CC_BRIGHTNESS] / 128.0 + 0.5;
    double brightness = exp(-osc_freq / (20000 * k * k * k * k * k * k));
    double osc = tick_osc(&osc_state, osc_freq * exp(0.000 * lfo1 + 0.000 * lfo2), brightness);
    double svf_freq = 440.0 * pow(2.0, (current_key - 69) / 12.0 * (instance->wrapper_cc[CC_TRACKING] / 127.0) + (instance->wrapper_cc[CC_CUTOFF] - 69 + 36) / 12.0);
    double svf_q = 1.0 - instance->wrapper_cc[CC_RESONANCE] / 127.0;
    double svf_grit = (1 + instance->wrapper_cc[CC_GRIT]) * (2.0/127.0);
    double svf = svf_tick_nonlinear(&svf_state, osc, svf_freq, svf_q, svf_grit);
    double env = svf_tick(&env_state, gain, env_freq, env_q);
    double volume = instance->wrapper_cc[CC_VOLUME] * instance->wrapper_cc[CC_VOLUME] / (127.0 * 127.0) * 0.25;
    audio_out_buf[i] = svf * env * volume;
  }
}

void plugin_process(struct instance* instance, int nframes) {
  jack_nframes_t num_events = jack_midi_get_event_count(midi_in_buf);
  int sample_index = 0;
  FOR(e, num_events) {
    jack_midi_event_t event;
    jack_midi_event_get(&event, midi_in_buf, e);
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
  wrapper_add_cc(instance, CC_BRIGHTNESS, "Brightness", "brightness", 0);
  wrapper_add_cc(instance, CC_TRACKING, "Tracking", "tracking", 64);
}

void plugin_destroy(struct instance* instance) {
}

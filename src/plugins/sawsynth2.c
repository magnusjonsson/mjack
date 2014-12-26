#include <jack/midiport.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

#include "../wrappers/wrapper.h"

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
#define CC_VCF_DECAY 126 // TODO assign

// dsp control values
static double gain;
static double vcf_env;
static double osc_freq;
static double env_freq;

struct lpf {
  double re;
  double im;
};

void lpf_tick(struct lpf *lpf, double pole_re, double pole_im, double dt, double input_A) {
  double phi_re = exp(pole_re * dt) * cos(pole_im * dt);
  double phi_im = exp(pole_re * dt) * sin(pole_im * dt);
  
  double new_re = lpf->re * phi_re - lpf->im * phi_im;
  double new_im = lpf->re * phi_im + lpf->im * phi_re;

  lpf->re = new_re + (1 - phi_re) * input_A;
  lpf->im = new_im - phi_im * input_A;
}

void lpf_add_step(struct lpf *lpf, double pole_re, double pole_im, double dt, double A) {
  double phi_re = exp(pole_re * dt) * cos(pole_im * dt);
  double phi_im = exp(pole_re * dt) * sin(pole_im * dt);
  lpf->re += (1 - phi_re) * A;
  lpf->im += -phi_im * A;
}

double lpf_sample(struct lpf *lpf) {
  return lpf->re;
}

// dsp state
struct osc_state {
  double phase;
  struct lpf lpf;
};

static struct osc_state osc_state;
static double env_state;

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
    env_freq = osc_freq * 0.25;
    gain = velocity / 127.0;
    vcf_env = 1.0;
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

static double tick_osc_square(struct osc_state* osc_state, double freq, double re, double im) {
  double curr_output = osc_state->phase < 0 ? -1 : 1;
  lpf_tick(&osc_state->lpf, re, im, dt, curr_output);

  osc_state->phase += freq * dt;
 again:
  if (curr_output < 0 && osc_state->phase > 0) {
    double fdt = (osc_state->phase - 0.0) / (freq);
    lpf_add_step(&osc_state->lpf, re, im, fdt, 2);
    curr_output = 1;
  }
  if (osc_state->phase >= 0.5) {
    double fdt = (osc_state->phase - 0.5) / (freq);
    lpf_add_step(&osc_state->lpf, re, im, fdt, -2);
    osc_state->phase -= 1;
    curr_output = -1;
    goto again;
  }

  return lpf_sample(&osc_state->lpf);
}

static double tick_osc_saw(struct osc_state* osc_state, double freq, double re, double im) {
  lpf_tick(&osc_state->lpf, re, im, dt, osc_state->phase + 0.5 * freq * dt);

  osc_state->phase += freq * dt;
 again:
  if (osc_state->phase >= 0.5) {
    double fdt = (osc_state->phase - 0.5) / (freq);
    lpf_add_step(&osc_state->lpf, re, im, fdt, -1);
    osc_state->phase -= 1;
    goto again;
  }

  return 2 * lpf_sample(&osc_state->lpf);
}

static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  double cutoff = 440.0 * pow(2.0, (current_key - 69) / 12.0 * (instance->wrapper_cc[CC_TRACKING] / 127.0) + (instance->wrapper_cc[CC_CUTOFF] - 69 + 24 + 4) / 12.0) * 3.141592;
  double reso = instance->wrapper_cc[CC_RESONANCE] / 128.0;
  double re = -cutoff * (1 - reso);
  double im = cutoff * reso;
  double volume = instance->wrapper_cc[CC_VOLUME] * instance->wrapper_cc[CC_VOLUME] / (127.0 * 127.0) * 0.25;
  double vcf_decay_rate = 10000 * pow(instance->wrapper_cc[CC_VCF_DECAY] / 127.0, 2);
  for (int i = start_frame; i < end_frame; ++i) {
    double env = env_state += (gain - env_state) * 2 * 3.141592 * env_freq * dt;
    vcf_env *= fmax(0.5, 1 - vcf_env * vcf_env * vcf_env * vcf_decay_rate * dt);
    double svf = tick_osc_saw(&osc_state, osc_freq, re * vcf_env, im * vcf_env);
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
  wrapper_add_cc(instance, CC_VCF_DECAY, "VCF Decay", "vcf_decay", 64);
  wrapper_add_cc(instance, CC_TRACKING, "Tracking", "tracking", 64);
}

void plugin_destroy(struct instance* instance) {
}

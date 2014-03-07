#include <jack/midiport.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "wrapper.h"
#include "osc.h"
#include "svf.h"
#include "ladder.h"
#include "sem_svf.h"

const char* plugin_name = "PolySaw";
const char* plugin_persistence_name = "mjack_polysaw";

#define NUM_VOICES 8

#define FOR(var,limit) for(int var = 0; var < limit; ++var)

static void* midi_in_buf;
static float* audio_out_buf;
static char key_is_pressed[128];
static int current_key[NUM_VOICES] = { -1 };

// constants
static double dt;

// settings

#define CC_VOLUME 7
#define CC_CUTOFF 77
#define CC_RESONANCE 71
#define CC_CLIP_LEVEL 72
#define CC_TRACKING 80
#define CC_VCF_PRE_GAIN 113
#define CC_VCA_PRE_GAIN 114
#define CC_OSC_MIX 115
#define CC_OCTAVE 116
#define CC_FIFTH 117
#define CC_EXP_DETUNE 118
#define CC_LIN_DETUNE 119

#define CC_VCA_ATTACK 120
#define CC_VCA_DECAY 121
#define CC_VCA_SUSTAIN 122
#define CC_VCA_RELEASE 123

#define CC_VCF_ATTACK 124
#define CC_VCF_DECAY 125
#define CC_VCF_SUSTAIN 126
#define CC_VCF_RELEASE 127

// dsp control values
static double gain[NUM_VOICES];
static double osc_freq[NUM_VOICES];
static double env_freq[NUM_VOICES];

// dsp state
static struct osc osc1[NUM_VOICES];
static struct osc osc2[NUM_VOICES];

static struct svf svf[NUM_VOICES];
static struct ladder ladder[NUM_VOICES];
static struct sem_svf sem_svf[NUM_VOICES];

struct env {
  bool in_attack;
  double value;
};
static struct env vca_env[NUM_VOICES];
static struct env vcf_env[NUM_VOICES];

static inline double env_tick(struct env *env, double dt, double attack, double decay, double sustain, double release, double gate, int shape) {
  if (gate > 0) {
    if (env->in_attack) {
      env->value += dt * attack * (1.25 - env->value);
      if (env->value >= 1.0) {
	env->value = 1.0;
	env->in_attack = false;
      }
    } else {
      double v = env->value - sustain;
      env->value = sustain + v * fmax(0.5, fmin(1.0, 1 - dt * decay * pow(v, shape)));
    }
  } else {
    env->value *= fmax(0.5, fmin(2.0, 1 - dt * release * pow(env->value, shape)));
  }
  return env->value;
}

static inline void env_trigger(struct env *env) {
  env->in_attack = true;
}

static void init(double sample_rate) {
  dt = 1.0 / sample_rate;
  printf("dt = %lg\n", dt);
}
static void handle_midi_note_off(int key, int velocity) {
  if (key_is_pressed[key]) {
    key_is_pressed[key] = 0;
    FOR(v, NUM_VOICES) {
      if (current_key[v] == key) {
	gain[v] = 0.0;
      }
    }
  }
}

struct {
  int octaves;
  int fifths;
} note_vector[12] = {
  {0,0},  // C
  {-4,7}, // C#
  {-1,2}, // D
  {2,-3}, // Eb
  {-2,4}, // E
  {1,-1}, // F
  {-3,6}, // F#
  {0,1},  // G
  {-4,8}, // G#
  {-1,3}, // A
  {2,-2}, // Bb
  {-2,5}, // B
};

static void handle_midi_note_on(struct instance* instance, int key, int velocity) {
  if (!key_is_pressed[key]) {
    key_is_pressed[key] = 1;
    FOR(v, NUM_VOICES) {
      if (gain[v] == 0.0) {
	current_key[v] = key;
	int note = key%12;
	int oct = key/12 - 5;

	double OCTAVE_CENTS = 1200.0 + 0.1 * (instance->wrapper_cc[CC_OCTAVE]-64);
	double FIFTH_CENTS = 700.0 + 0.1 * (instance->wrapper_cc[CC_FIFTH]-64);

	osc_freq[v] = 440.0 * pow(2.0, (OCTAVE_CENTS * (oct + note_vector[note].octaves - note_vector[9].octaves) + FIFTH_CENTS * (note_vector[note].fifths - note_vector[9].fifths)) / 1200.0);
	env_freq[v] = osc_freq[v] * 1.0;
	env_trigger(&vca_env[v]);
	env_trigger(&vcf_env[v]);
	gain[v] = velocity / 127.0;
	break;
      }
    }
  }
}

static void handle_midi_event(struct instance* instance, const jack_midi_event_t* event) {
  switch (event->buffer[0] & 0xf0) {
  case 0x80:
    handle_midi_note_off(event->buffer[1], event->buffer[2]);
    break;
  case 0x90:
    if (event->buffer[2] == 0) {
      handle_midi_note_off(event->buffer[1], 64);
    } else {
      handle_midi_note_on(instance, event->buffer[1], event->buffer[2]);
    }
    break;
  }
}

static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  for(int i = start_frame; i < end_frame; ++i) {
    audio_out_buf[i] = 0.0;
  }
  double osc_mix = instance->wrapper_cc[CC_OSC_MIX] / 127.0;
  double exp_detune = 0.005 * pow(instance->wrapper_cc[CC_EXP_DETUNE] / 128.0 + 0.5, 2);
  double lin_detune = 4 * pow(instance->wrapper_cc[CC_LIN_DETUNE] / 128.0 + 0.5, 4);
  double vca_attack  = pow(instance->wrapper_cc[CC_VCA_ATTACK] / 128.0, 4) * 1000;
  double vca_decay   = pow(instance->wrapper_cc[CC_VCA_DECAY] / 128.0, 4) * 1000;
  double vca_sustain = pow(instance->wrapper_cc[CC_VCA_SUSTAIN] / 128.0, 2);
  double vca_release = pow(instance->wrapper_cc[CC_VCA_RELEASE] / 128.0, 4) * 1000;
  double vcf_attack  = pow(instance->wrapper_cc[CC_VCF_ATTACK] / 128.0, 4) * 1000;
  double vcf_decay   = pow(instance->wrapper_cc[CC_VCF_DECAY] / 128.0, 4) * 1000;
  double vcf_sustain = pow(instance->wrapper_cc[CC_VCF_SUSTAIN] / 128.0, 2);
  double vcf_release = pow(instance->wrapper_cc[CC_VCF_RELEASE] / 128.0, 4) * 1000;

  double vcf_pregain = pow(instance->wrapper_cc[CC_VCF_PRE_GAIN] / 64.0, 2);
  double vca_pregain = pow(instance->wrapper_cc[CC_VCA_PRE_GAIN] / 64.0, 2);
  double svf_q = 1.0 - instance->wrapper_cc[CC_RESONANCE] / 127.0;
  double ladder_reso = instance->wrapper_cc[CC_RESONANCE] / 127.0;
  double vcf_clip_level = pow((instance->wrapper_cc[CC_CLIP_LEVEL] + 1) / 64.0, 2);
  double volume = instance->wrapper_cc[CC_VOLUME] * instance->wrapper_cc[CC_VOLUME] / (127.0 * 127.0) * 0.25;
  FOR(v, NUM_VOICES) {
    double svf_freq = 440.0 * pow(osc_freq[v]/440.0, instance->wrapper_cc[CC_TRACKING] / 127.0) * pow(2.0, (instance->wrapper_cc[CC_CUTOFF] - 69 + 24 + 4) / 12.0);
    for (int i = start_frame; i < end_frame; ++i) {
      double osc1_out = osc_tick(&osc1[v], dt, osc_freq[v] * (1.0 - exp_detune));
      double osc2_out = osc_tick(&osc2[v], dt, osc_freq[v] * (1.0 + exp_detune) + lin_detune);
      double osc_out = osc1_out * (1 - osc_mix) + osc2_out * osc_mix;
      double vcf_env_out = env_tick(&vcf_env[v], dt, vcf_attack, vcf_decay, vcf_sustain, vcf_release, gain[v], 2);
      double f = svf_freq * vcf_env_out;
      //double svf_out = ladder_tick(&ladder[v], dt, f, f, f, f, 5 * ladder_reso, vcf_clip_level, osc_out * vcf_pregain);
      double svf_out = sem_svf_tick(&sem_svf[v], dt, f, svf_q, vcf_clip_level, osc_out * vcf_pregain);
      double vca_env_out = env_tick(&vca_env[v], dt, vca_attack, vca_decay, vca_sustain, vca_release, gain[v], 0);
      audio_out_buf[i] += sem_svf_shape(1.0, svf_out * vca_pregain) * vca_env_out * volume;
    }
  }
}

void plugin_process(struct instance* instance, int nframes) {
  jack_nframes_t num_events = jack_midi_get_event_count(midi_in_buf);
  int sample_index = 0;
  FOR(e, num_events) {
    jack_midi_event_t event;
    jack_midi_event_get(&event, midi_in_buf, e);
    handle_midi_event(instance, &event);
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
  wrapper_add_cc(instance, CC_VCF_PRE_GAIN, "VCF Pre-Gain", "vcf_pre_gain", 64);
  wrapper_add_cc(instance, CC_VCA_PRE_GAIN, "VCA Pre-Gain", "vca_pre_gain", 64);
  wrapper_add_cc(instance, CC_VCA_ATTACK, "VCA Attack", "vca_attack", 64);
  wrapper_add_cc(instance, CC_VCA_DECAY, "VCA Decay", "vca_decay", 64);
  wrapper_add_cc(instance, CC_VCA_SUSTAIN, "VCA Sustain", "vca_sustain", 64);
  wrapper_add_cc(instance, CC_VCA_RELEASE, "VCA Release", "vca_release", 64);

  wrapper_add_cc(instance, CC_CUTOFF, "Cutoff", "cutoff", 64);
  wrapper_add_cc(instance, CC_RESONANCE, "Resonance", "resonance", 64);
  wrapper_add_cc(instance, CC_CLIP_LEVEL, "Clip Level", "clip_level", 64);
  wrapper_add_cc(instance, CC_TRACKING, "Tracking", "tracking", 0);
  wrapper_add_cc(instance, CC_VCF_ATTACK, "VCF Attack", "vcf_attack", 64);
  wrapper_add_cc(instance, CC_VCF_DECAY, "VCF Decay", "vcf_decay", 64);
  wrapper_add_cc(instance, CC_VCF_SUSTAIN, "VCF Sustain", "vcf_sustain", 64);
  wrapper_add_cc(instance, CC_VCF_RELEASE, "VCF Release", "vcf_release", 64);
  wrapper_add_cc(instance, CC_OSC_MIX, "Osc Mix", "osc_mix", 0);
  wrapper_add_cc(instance, CC_EXP_DETUNE, "Exp Detune", "exp_detune", 0);
  wrapper_add_cc(instance, CC_LIN_DETUNE, "Lin Detune", "lin_detune", 0);
  wrapper_add_cc(instance, CC_OCTAVE, "Octave", "octave", 64);
  wrapper_add_cc(instance, CC_FIFTH, "Fifth", "fifth", 64);
}

void plugin_destroy(struct instance* instance) {
}

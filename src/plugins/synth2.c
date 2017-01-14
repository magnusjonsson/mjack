#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../wrappers/wrapper.h"
#include "../dsp/osc.h"
#include "../dsp/ladder.h"
#include "../dsp/ms20-filter.h"
#include "../dsp/lfo.h"
#include "../dsp/adsr-env.h"
#include "../dsp/simple-env.h"
#include "../tuning/meantone.h"

const char* plugin_name = "Synth2";
const char* plugin_persistence_name = "mjack_synth2";

#define NUM_VOICES 8

#define FOR(var,limit) for(int var = 0; var < limit; ++var)

// TODO migrate all state into instance->plugin

static void* midi_in_buf;
static float* audio_out_buf;
static char key_is_pressed[128];
static int current_key[NUM_VOICES] = { -1 };

// constants
static double dt;
static double recip_sqrt_dt;

// settings

#define KNOBS \
  X(CC_OSC_MIX, 81, "Osc Mix") \
  X(CC_OSC_PULSE_WIDTH, 82, "Pulse Width") \
  X(CC_OSC_PWM_DEPTH, 83, "PWM Depth") \
  X(CC_OSC_PWM_FREQUENCY, 84, "PWM Rate") \
  X(CC_OSC_VIBRATO_DEPTH, 85, "Vibrato Depth") \
  X(CC_OSC_VIBRATO_FREQUENCY, 86, "Vibrato Rate") \
  X(CC_OSC_VIBRATO_DELAY, 87, "Vibrato Delay") \
\
  X(CC_LPF_CUTOFF, 88, "LPF Cutoff")	  \
  X(CC_LPF_RESONANCE, 89, "LPF Resonance")	\
  X(CC_LPF_TRACKING, 90, "LPF Tracking")	\
  X(CC_LPF_ENV1, 91, "LPF Env1")		\
  X(CC_LPF_ENV2, 92, "LPF Env2")		\
\
  X(CC_ENV1_SPEED, 94, "Env1 Speed")		\
  X(CC_ENV1_DECAY, 95, "Env1 Decay")		\
\
  X(CC_ENV2_SPEED, 96, "Env2 Speed")		\
  X(CC_ENV2_DECAY, 97, "Env2 Decay")		\
\
  X(CC_VOLUME_ENV_MIX, 98, "Mix") \
  X(CC_DRIVE, 99, "Drive") \
  X(CC_VOLUME, 100, "Volume") \
  X(CC_OCTAVE, 101, "Octave") \
  X(CC_FIFTH, 102, "Fifth") \
  X(CC_LIN_DRIFT, 103, "Lin Drift") \
  X(CC_LOG_DRIFT, 104, "Log Drift") \
  X(CC_DRIFT_CUTOFF, 105, "Drift Cufoff") \

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};

// keyboard values
static double gain[NUM_VOICES];

// dsp state
static struct osc osc[NUM_VOICES];
static struct lfo osc_pwm_lfo[NUM_VOICES];
static struct lfo osc_vibrato_lfo[NUM_VOICES];
static struct ladder lpf[NUM_VOICES];
static struct simple_env env1[NUM_VOICES];
static struct simple_env env2[NUM_VOICES];

static void init(double sample_rate) {
  dt = 1.0 / sample_rate;
  recip_sqrt_dt = 1.0 / sqrt(dt);
  printf("dt = %lg\n", dt);
}
static void handle_midi_note_off(int key, int velocity) {
  if (key_is_pressed[key]) {
    key_is_pressed[key] = 0;
    FOR(v, NUM_VOICES) {
      if (current_key[v] == key) {
	gain[v] = 0.0;
	simple_env_untrigger(&env1[v]);
	simple_env_untrigger(&env2[v]);
      }
    }
  }
}

static int next_voice;

static void handle_midi_note_on(struct instance* instance, int key, int velocity) {
  if (!key_is_pressed[key]) {
    key_is_pressed[key] = 1;
    FOR(count, NUM_VOICES) {
      int v = next_voice++;
      if (next_voice >= NUM_VOICES) next_voice = 0;
      if (gain[v] == 0.0) {
	current_key[v] = key;
	simple_env_trigger(&env1[v]);
	simple_env_trigger(&env2[v]);
	lfo_trigger(&osc_vibrato_lfo[v]);
	gain[v] = velocity / 127.0;
	return;
      }
    }
  }
}

static void handle_midi_event(struct instance* instance, const struct midi_event* event) {
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
  case 0xB0:
    fprintf(stderr, "Received CC %i -> %i\n", event->buffer[1], event->buffer[2]);
    instance->wrapper_cc[event->buffer[1]] = event->buffer[2];
    break;
  }
}

static double frand(void) {
  return -1.0 + rand() * 2.0 / RAND_MAX;
}

static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  for(int i = start_frame; i < end_frame; ++i) {
    audio_out_buf[i] = 0.0;
  }

  double osc_mix = instance->wrapper_cc[CC_OSC_MIX] / 127.0;
  double osc_pulse_width = pow(instance->wrapper_cc[CC_OSC_PULSE_WIDTH] / 128.0, 2);
  double osc_pwm_freq = 20 * pow(instance->wrapper_cc[CC_OSC_PWM_FREQUENCY] / 127.0, 2);
  double osc_pwm_depth = instance->wrapper_cc[CC_OSC_PWM_DEPTH] / 128.0;
  double osc_vibrato_delay = 3 * pow(instance->wrapper_cc[CC_OSC_VIBRATO_DELAY] / 127.0, 2);
  double osc_vibrato_freq = 20 * pow(instance->wrapper_cc[CC_OSC_VIBRATO_FREQUENCY] / 127.0, 2);
  double osc_vibrato_depth = 1 * pow(instance->wrapper_cc[CC_OSC_VIBRATO_DEPTH] / 128.0, 2);
  double env1_speed = pow(instance->wrapper_cc[CC_ENV1_SPEED] / 128.0, 2) * 10000;
  double env2_speed = pow(instance->wrapper_cc[CC_ENV2_SPEED] / 128.0, 2) * 10000;
  double env1_decay  = pow(instance->wrapper_cc[CC_ENV1_DECAY] / 128.0, 4) * 100;
  double env2_decay  = pow(instance->wrapper_cc[CC_ENV2_DECAY] / 128.0, 4) * 100;

  double lpf_pitch = instance->wrapper_cc[CC_LPF_CUTOFF] - (69-24);
  double lpf_reso = instance->wrapper_cc[CC_LPF_RESONANCE] / 127.0;
  double lpf_tracking = instance->wrapper_cc[CC_LPF_TRACKING] / 127.0;
  double lpf_env1 = pow(instance->wrapper_cc[CC_LPF_ENV1] / 127.0, 2) * 127;
  double lpf_env2 = pow(instance->wrapper_cc[CC_LPF_ENV2] / 127.0, 2) * 127;

  double volume_env_mix = instance->wrapper_cc[CC_VOLUME_ENV_MIX] / 127.0;
  double drive = pow(instance->wrapper_cc[CC_DRIVE] / 127.0, 2) * 4;
  double gain = pow(instance->wrapper_cc[CC_VOLUME] / 127.0, 2);

  double octave_cents = 1200.0 + 0.1 * (instance->wrapper_cc[CC_OCTAVE] - 64);
  double fifth_cents = 700.0 + 0.1 * (instance->wrapper_cc[CC_FIFTH] - 64);
   
  double drift_coeff = dt * 2 * 3.141592 * 440.0 * pow((instance->wrapper_cc[CC_DRIFT_CUTOFF] + 1.0) / 128.0, 2.0);

  FOR(v, NUM_VOICES) {
    double pitch = meantone_cents(current_key[v], octave_cents, fifth_cents) * 0.01;

    for (int i = start_frame; i < end_frame; ++i) {
      double env1_out = simple_env_tick(&env1[v], dt, env1_speed, env1_decay);
      double env2_out = simple_env_tick(&env2[v], dt, env2_speed, env2_decay);

      double osc_final_pw = osc_pulse_width + osc_pwm_depth * lfo_tick(&osc_pwm_lfo[v], dt, 0, osc_pwm_freq);
      double vibrato_lfo_out = lfo_tick(&osc_vibrato_lfo[v], dt, osc_vibrato_delay, osc_vibrato_freq);
      double lin_drift_noise = instance->wrapper_cc[CC_LIN_DRIFT] / 127.0 * frand();
      double log_drift_noise = instance->wrapper_cc[CC_LOG_DRIFT] / 127.0 * frand() * 0.01;

      static double lin_drift_lpf[NUM_VOICES];
      static double log_drift_lpf[NUM_VOICES];

      lin_drift_lpf[v] += (lin_drift_noise * recip_sqrt_dt - lin_drift_lpf[v]) * drift_coeff;
      log_drift_lpf[v] += (log_drift_noise * recip_sqrt_dt - log_drift_lpf[v]) * drift_coeff;
      double osc_final_freq = 440.0 * pow(2, (pitch + osc_vibrato_depth * vibrato_lfo_out) / 12.0) * (1 + log_drift_lpf[v]) + lin_drift_lpf[v];

      double saw_out = osc_tick(&osc[v], dt, osc_final_freq);
      double pulse_out = saw_out > osc_final_pw ? 0.6 : -0.6;
      double osc_out = (1 - osc_mix) * saw_out + osc_mix * pulse_out;

      double lpf_final_cutoff = 440.0 * pow(2.0, (lpf_pitch + lpf_tracking * (pitch + 24.0) + lpf_env1 * env1_out + lpf_env2 * env2_out) / 12.0);

      //double lpf_out = ladder_tick(&lpf[v], dt, lpf_final_cutoff, 9 * lpf_reso, 1.0, osc_out);
      double lpf_out = ladder_tick_nonlinear_blt(&lpf[v], dt, lpf_final_cutoff, 9 * lpf_reso, 1., osc_out * 0.5);

      double volume = (1 - volume_env_mix) * env1_out + volume_env_mix * env2_out;
      audio_out_buf[i] += tanh(lpf_out * volume * drive * 4) * 0.25 * gain;
    }
  }
}

void plugin_process(struct instance* instance, int nframes) {
  int num_events = wrapper_get_num_midi_events(midi_in_buf);
  int sample_index = 0;
  FOR(e, num_events) {
    struct midi_event event = wrapper_get_midi_event(midi_in_buf, e);
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

#define X(name, value, label) wrapper_add_cc(instance, value, label, #name, 64);
  KNOBS
#undef X
}

void plugin_destroy(struct instance* instance) {
}

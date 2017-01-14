#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "../wrappers/wrapper.h"
#include "../dsp/biquad.h"

const char* plugin_name = "SawSynth4";
const char* plugin_persistence_name = "mjack_sawsynth_4";

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

#define CC_OSC_VOLUME_A 10
#define CC_OSC_VOLUME_B 11
#define CC_OSC_VOLUME_C 12
#define CC_OSC_DETUNE_A 20
#define CC_OSC_DETUNE_B 21
#define CC_OSC_DETUNE_C 22
#define CC_BP_VOLUME_A 7
#define CC_BP_VOLUME_B 8
#define CC_BP_VOLUME_C 9
#define CC_BP_CUTOFF_A 77
#define CC_BP_CUTOFF_B 78
#define CC_BP_CUTOFF_C 79
#define CC_BP_RESONANCE_A 71
#define CC_BP_RESONANCE_B 72
#define CC_BP_RESONANCE_C 73
#define CC_PORTAMENTO 81
#define CC_ENVELOPE 82
#define CC_DECAY 83
#define CC_VIBRATO_DEPTH 90
#define CC_VIBRATO_RATE 91
#define CC_VIBRATO_ENVELOPE 92

#define NUM_OSC 3
#define NUM_BP 3

// dsp control values
static double gain;
static double osc_target_freq;
static double osc_freq;

// dsp state

struct osc_state {
  double phase;
};

static double osc_tick(struct osc_state *_, double freq) {
  _->phase += freq * dt;
  if (_->phase >= -0.5) {
    _->phase -= 1;
  }
  return _->phase * _->phase * 4 - 1.0/3.0;
}

static struct osc_state osc_state[NUM_OSC];
static struct biquad_state bp_state[NUM_BP];

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

static void handle_midi_note_on(struct instance *instance, int key, int velocity) {
  printf("note on\n");
  if (!key_is_pressed[key]) {
    key_is_pressed[key] = 1;
    ++key_count_pressed;
    current_key = key;
    osc_target_freq = instance->freq[key];
    gain = velocity / 127.0;
  }
}

static void handle_midi_event(struct instance *instance, const struct midi_event *event) {
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

static double hpf_state;

static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  const double osc_gain[NUM_OSC] = {
    instance->wrapper_cc[CC_OSC_VOLUME_A] / 127.0,
    instance->wrapper_cc[CC_OSC_VOLUME_B] / 127.0,
    instance->wrapper_cc[CC_OSC_VOLUME_C] / 127.0,
  };
  const double osc_detune[NUM_OSC] = {
    0.02 * (instance->wrapper_cc[CC_OSC_DETUNE_A] - 64) / 128.0,
    0.02 * (instance->wrapper_cc[CC_OSC_DETUNE_B] - 64) / 128.0,
    0.02 * (instance->wrapper_cc[CC_OSC_DETUNE_C] - 64) / 128.0,
  };
  const double bp_gain[NUM_BP] = {
    instance->wrapper_cc[CC_BP_VOLUME_A] * instance->wrapper_cc[CC_BP_VOLUME_A] / (127.0 * 127.0) * 0.5,
    instance->wrapper_cc[CC_BP_VOLUME_B] * instance->wrapper_cc[CC_BP_VOLUME_B] / (127.0 * 127.0) * 0.5,
    instance->wrapper_cc[CC_BP_VOLUME_C] * instance->wrapper_cc[CC_BP_VOLUME_C] / (127.0 * 127.0) * 0.5,
  };
  const double bp_freq[NUM_BP] = {
    440.0 * pow(2.0, (instance->wrapper_cc[CC_BP_CUTOFF_A] - 69 + 12) / 12.0),
    440.0 * pow(2.0, (instance->wrapper_cc[CC_BP_CUTOFF_B] - 69 + 12) / 12.0),
    440.0 * pow(2.0, (instance->wrapper_cc[CC_BP_CUTOFF_C] - 69 + 12) / 12.0),
  };
  double bp_q[NUM_BP] = {
    4 * pow((instance->wrapper_cc[CC_BP_RESONANCE_A]+1) / 127.0, 2),
    4 * pow((instance->wrapper_cc[CC_BP_RESONANCE_B]+1) / 127.0, 2),
    4 * pow((instance->wrapper_cc[CC_BP_RESONANCE_C]+1) / 127.0, 2),
  };

  double portamento_speed = 300 * pow(instance->wrapper_cc[CC_PORTAMENTO] / 127.0, 2);
  double envelope_speed = 300 * pow(instance->wrapper_cc[CC_ENVELOPE] / 127.0, 3);
  double decay_speed = 50 * pow(instance->wrapper_cc[CC_DECAY] / 127.0, 4);
  double vibrato_depth = 0.015 * pow(instance->wrapper_cc[CC_VIBRATO_DEPTH] / 127.0, 2);
  double vibrato_rate = 15 * pow(instance->wrapper_cc[CC_VIBRATO_RATE] / 127.0, 2);
  double vibrato_env_speed = 10 * pow(instance->wrapper_cc[CC_VIBRATO_ENVELOPE] / 127.0, 3);

  struct biquad_params bp_params[NUM_BP];

  FOR(i, NUM_BP) {
    bp_params[i] = (struct biquad_params) {
      .w = bp_freq[i] * 2 * 3.141592 * dt,
      .Q = bp_q[i] * sqrt(bp_freq[i] / 440),
      .g0 = 0,
      .g1 = bp_gain[i],
      .g2 = 0,
    };
  }
  struct biquad_coeffs bp_coeffs[NUM_BP];
  FOR(i, NUM_BP) {
    bp_coeffs[i] = biquad_digital_parametric_asymmetric(bp_params[i]);
  }
  
  for (int i = start_frame; i < end_frame; ++i) {
    static double vibrato_env_state;
    vibrato_env_state += ((gain > 0 ? 1 : 0) - vibrato_env_state) * 2 * 3.141592 * vibrato_env_speed * dt;
    static double vibrato_phase;
    vibrato_phase += vibrato_rate * vibrato_env_state * dt;
    if (vibrato_phase > 0.5) { vibrato_phase -= 1.0; }
    osc_freq += (osc_target_freq - osc_freq) * portamento_speed * dt;
    double osc = 0.0;
    FOR(j, NUM_OSC) {
      osc += osc_gain[j] * osc_tick(&osc_state[j], osc_freq * (1
							       + osc_detune[j]
							       + vibrato_depth * vibrato_env_state * sin(vibrato_phase * 2 * 3.141592)));
    }
    static double env_state;
    double env = env_state += (gain - env_state) * 2 * 3.141592 * envelope_speed * dt;
    gain -= gain * dt * decay_speed;
    float output = 0.0;
    FOR(j, NUM_BP) {
      output += biquad_tick(bp_coeffs[j], &bp_state[j], osc * env);
    }
    hpf_state += (output - hpf_state) * 100 * dt;
    audio_out_buf[i] = output - hpf_state;
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
  wrapper_add_cc(instance, CC_OSC_VOLUME_A, "Osc Volume A", "osc_volume_a", 64);
  wrapper_add_cc(instance, CC_OSC_VOLUME_B, "Osc Volume B", "osc_volume_b", 64);
  wrapper_add_cc(instance, CC_OSC_VOLUME_C, "Osc Volume C", "osc_volume_c", 64);
  wrapper_add_cc(instance, CC_OSC_DETUNE_A, "Osc Detune A", "osc_detune_a", 64);
  wrapper_add_cc(instance, CC_OSC_DETUNE_B, "Osc Detune B", "osc_detune_b", 64);
  wrapper_add_cc(instance, CC_OSC_DETUNE_C, "Osc Detune C", "osc_detune_c", 64);
  wrapper_add_cc(instance, CC_BP_VOLUME_A, "BP Volume A", "bp_volume_a", 64);
  wrapper_add_cc(instance, CC_BP_VOLUME_B, "BP Volume B", "bp_volume_b", 64);
  wrapper_add_cc(instance, CC_BP_VOLUME_C, "BP Volume C", "bp_volume_c", 64);
  wrapper_add_cc(instance, CC_BP_CUTOFF_A, "Bp Cutoff A", "bp_cutoff_a", 64);
  wrapper_add_cc(instance, CC_BP_CUTOFF_B, "Bp Cutoff B", "bp_cutoff_b", 64);
  wrapper_add_cc(instance, CC_BP_CUTOFF_C, "Bp Cutoff C", "bp_cutoff_c", 64);
  wrapper_add_cc(instance, CC_BP_RESONANCE_A, "Bp Resonance_A", "bp_resonance_a", 64);
  wrapper_add_cc(instance, CC_BP_RESONANCE_B, "Bp Resonance_B", "bp_resonance_b", 64);
  wrapper_add_cc(instance, CC_BP_RESONANCE_C, "Bp Resonance_C", "bp_resonance_c", 64);
  wrapper_add_cc(instance, CC_PORTAMENTO, "Portamento", "portamento", 64);
  wrapper_add_cc(instance, CC_ENVELOPE, "Envelope", "envelope", 64);
  wrapper_add_cc(instance, CC_DECAY, "Decay", "Decay", 64);
  wrapper_add_cc(instance, CC_VIBRATO_RATE, "Vibrato Rate", "vibrato_rate", 64);
  wrapper_add_cc(instance, CC_VIBRATO_DEPTH, "Vibrato Depth", "vibrato_depth", 64);
  wrapper_add_cc(instance, CC_VIBRATO_ENVELOPE, "Vibrato Envelope", "vibrato_envelope", 64);
}

void plugin_destroy(struct instance* instance) {
}

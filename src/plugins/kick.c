#include <jack/midiport.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Kick";
const char* plugin_persistence_name = "mjack_kick";

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

#define NUM_OSCS 4

#define CC_AMP_DECAY 80
#define CC_START_FREQ 81
#define CC_AMP_DECAY_2 82
#define CC_START_FREQ_2 83
#define CC_AMP_DECAY_3 84
#define CC_START_FREQ_3 85
#define CC_AMP_DECAY_4 86
#define CC_START_FREQ_4 87
#define CC_FINE_TUNE 90


// dsp control values
static bool trigger;

// dsp state
static double complex state[NUM_OSCS];

static void handle_midi_note_off(int key, int velocity) {
  if (key_is_pressed[key]) {
    key_is_pressed[key] = 0;
    --key_count_pressed;
  }
}

static void handle_midi_note_on(int key, int velocity) {
  if (!key_is_pressed[key]) {
    key_is_pressed[key] = 1;
    ++key_count_pressed;
    current_key = key;
    FOR(i, NUM_OSCS) {
      state[i] += velocity / 127.0;
    }
    trigger = true;
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

static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  int freq_cc[NUM_OSCS];
  double freq[NUM_OSCS];
  double fine = (instance->wrapper_cc[CC_FINE_TUNE] - 64) / 64.0;
  freq_cc[0] = instance->wrapper_cc[CC_START_FREQ];
  freq_cc[1] = instance->wrapper_cc[CC_START_FREQ_2];
  freq_cc[2] = instance->wrapper_cc[CC_START_FREQ_3];
  freq_cc[3] = instance->wrapper_cc[CC_START_FREQ_4];
  FOR(j, NUM_OSCS) {
    freq[j] = 440.0 * pow(2.0, (freq_cc[j] - 69 + current_key - 69 + fine) / 12.0);
  }
  const double amp_decay[NUM_OSCS] = {
    pow(instance->wrapper_cc[CC_AMP_DECAY] / 128.0, 4)*1000,
    pow(instance->wrapper_cc[CC_AMP_DECAY_2] / 128.0, 4)*1000,
    pow(instance->wrapper_cc[CC_AMP_DECAY_3] / 128.0, 4)*1000,
    pow(instance->wrapper_cc[CC_AMP_DECAY_4] / 128.0, 4)*1000,
  };
  double complex step[NUM_OSCS];
  FOR(j, NUM_OSCS) step[j] = cexp(-dt * freq[j] * (2.0 * 3.151492i + amp_decay[j]));
  for (int i = start_frame; i < end_frame; ++i) {
    FOR(j, NUM_OSCS) state[j] *= step[j];
    double sum = 0;
    FOR(j, NUM_OSCS) sum += cimag(state[j]);
    audio_out_buf[i] = sum;
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
  dt = 1.0 / sample_rate;
  wrapper_add_midi_input(instance, "midi in", &midi_in_buf);
  wrapper_add_audio_output(instance, "audio out", &audio_out_buf);
  wrapper_add_cc(instance, CC_AMP_DECAY, "Amp Decay", "amp_decay", 64);
  wrapper_add_cc(instance, CC_START_FREQ, "Freq Start", "freq_start", 64);
  wrapper_add_cc(instance, CC_AMP_DECAY_2, "Amp Decay 2", "amp_decay_2", 64);
  wrapper_add_cc(instance, CC_START_FREQ_2, "Freq Start 2", "freq_start_2", 64);
  wrapper_add_cc(instance, CC_AMP_DECAY_3, "Amp Decay 3", "amp_decay_3", 64);
  wrapper_add_cc(instance, CC_START_FREQ_3, "Freq Start 3", "freq_start_3", 64);
  wrapper_add_cc(instance, CC_AMP_DECAY_4, "Amp Decay 4", "amp_decay_4", 64);
  wrapper_add_cc(instance, CC_START_FREQ_4, "Freq Start 4", "freq_start_4", 64);
  wrapper_add_cc(instance, CC_FINE_TUNE, "Fine Tune", "fine_tune", 64);
}

void plugin_destroy(struct instance* instance) {
}

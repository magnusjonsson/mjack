#include <jack/midiport.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "FM";
const char* plugin_persistence_name = "mjack_fm";

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

#define CC_AMP_DECAY 80
#define CC_FREQ_DECAY 81
#define CC_START_FREQ 82
#define CC_AMP_DECAY_2 83
#define CC_FREQ_DECAY_2 84
#define CC_START_FREQ_2 85
#define CC_MOD_11 86
#define CC_MOD_12 87
#define CC_MOD_21 88
#define CC_MOD_22 89
#define CC_FINE_TUNE 90


// dsp control values
static double amp[2];
static bool trigger;

// dsp state
static double phase[2];
static double freq[2];

static void handle_midi_note_off(int key, int velocity) {
  if (key_is_pressed[key]) {
    key_is_pressed[key] = 0;
    --key_count_pressed;
    if (key_count_pressed == 0) {
    }
  }
}

static void handle_midi_note_on(int key, int velocity) {
  if (!key_is_pressed[key]) {
    key_is_pressed[key] = 1;
    ++key_count_pressed;
    current_key = key;
    amp[0] = velocity / 127.0;
    amp[1] = velocity / 127.0;
    phase[0] = 0;
    phase[1] = 0;
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
  if (trigger) {
    double fine = (instance->wrapper_cc[CC_FINE_TUNE] - 64) / 64.0;
    freq[0] = 440.0 * pow(2.0, (instance->wrapper_cc[CC_START_FREQ] - 69 + current_key - 69 + fine) / 12.0);
    freq[1] = 440.0 * pow(2.0, (instance->wrapper_cc[CC_START_FREQ_2] - 69 + current_key - 69 + fine) / 12.0);
    trigger = false;
  }
  const double freq_decay[2] = {
    pow(instance->wrapper_cc[CC_FREQ_DECAY] / 128.0, 4) * 100,
    pow(instance->wrapper_cc[CC_FREQ_DECAY_2] / 128.0, 4) * 100,
  };
  const double amp_decay[2] = {
    pow(instance->wrapper_cc[CC_AMP_DECAY] / 128.0, 4) * 1000,
    pow(instance->wrapper_cc[CC_AMP_DECAY_2] / 128.0, 4) * 1000,
  };
  const double mod[2][2] = {
    {
      pow(instance->wrapper_cc[CC_MOD_11] / 128.0, 4) * 8,
      pow(instance->wrapper_cc[CC_MOD_12] / 128.0, 4) * 8,
    },
    {
      pow(instance->wrapper_cc[CC_MOD_21] / 128.0, 4) * 8,
      pow(instance->wrapper_cc[CC_MOD_22] / 128.0, 4) * 8,
    },
  };
  for (int i = start_frame; i < end_frame; ++i) {
    FOR(j, 2) phase[j] += freq[j] * dt;
    FOR(j, 2) if (phase[j] >= 1) phase[j] -= 1;
    static double out[2];
    FOR(j, 2) {
      double fm_j = 0;
      FOR(k, 2) {
	fm_j += mod[j][k] * out[k];
      }
      out[j] = sin(2*3.141592*(phase[j] - fm_j)) * amp[j];
    }
    FOR(j, 2) freq[j] *= fmax(0, 1 - freq[j] * dt * freq_decay[j]);
    FOR(j, 2) amp[j] *= fmax(0, 1 - dt * amp_decay[j]);
    double sum = 0;
    FOR(j, 2) sum += out[j];
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
  wrapper_add_cc(instance, CC_FREQ_DECAY, "Freq Decay", "freq_decay", 64);
  wrapper_add_cc(instance, CC_START_FREQ, "Freq Start", "freq_start", 64);
  wrapper_add_cc(instance, CC_AMP_DECAY_2, "Amp Decay 2", "amp_decay_2", 64);
  wrapper_add_cc(instance, CC_FREQ_DECAY_2, "Freq Decay 2", "freq_decay_2", 64);
  wrapper_add_cc(instance, CC_START_FREQ_2, "Freq Start 2", "freq_start_2", 64);
  wrapper_add_cc(instance, CC_MOD_11, "FM 1<-1", "fm_11", 64);
  wrapper_add_cc(instance, CC_MOD_12, "FM 1<-2", "fm_12", 64);
  wrapper_add_cc(instance, CC_MOD_21, "FM 2<-1", "fm_21", 64);
  wrapper_add_cc(instance, CC_MOD_22, "FM 2<-2", "fm_22", 64);
  wrapper_add_cc(instance, CC_FINE_TUNE, "Fine Tune", "fine_tune", 64);
}

void plugin_destroy(struct instance* instance) {
}

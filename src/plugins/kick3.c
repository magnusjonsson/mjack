#include <jack/midiport.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <float.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Kick3";
const char* plugin_persistence_name = "mjack_kick3";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif 

#define FOR(var,limit) for(int var = 0; var < limit; ++var)

static void* midi_in_buf;
static float* audio_out_buf;

// constants
static double dt;

// settings

#define NUM_OSCS 4

#define CC_FREQ_A 80
#define CC_FREQ_B 81
#define CC_FREQ_C 82
#define CC_FREQ_D 83
#define CC_AMP_A 90
#define CC_AMP_B 91
#define CC_AMP_C 92
#define CC_AMP_D 93
#define CC_DECAY_A 100
#define CC_DECAY_B 101
#define CC_DECAY_C 102
#define CC_DECAY_D 103
#define CC_RELEASE_A 110
#define CC_RELEASE_B 111
#define CC_RELEASE_C 112
#define CC_RELEASE_D 113
#define CC_FINE_TUNE 120

#define NUM_VOICES 8

static bool gate[NUM_VOICES];
static int key[NUM_VOICES];
static double complex osc[NUM_VOICES][NUM_OSCS];

// dsp state

static void handle_midi_note_off(int note_off_key, int velocity) {
  FOR(v, NUM_VOICES) {
    if (key[v] == note_off_key) {
      gate[v] = false;
    }
  }
}

static int alloc_voice(int new_key) {
  FOR(v, NUM_VOICES) {
    if (key[v] == new_key) {
      return v;
    }
  }
  double min_energy = DBL_MAX;
  int min_voice = 0;
  FOR(v, NUM_VOICES) {
    double energy = 0;
    FOR(o, NUM_OSCS) {
      double re = creal(osc[v][o]);
      double im = cimag(osc[v][o]);
      energy += re*re + im*im;
    }
    if (energy < min_energy) {
      min_energy = energy;
      min_voice = v;
    }
  }
  return min_voice;
}

static void handle_midi_note_on(struct instance *instance, int note_on_key, int velocity) {
  int v = alloc_voice(note_on_key);
  key[v] = note_on_key;
  gate[v] = true;
  double amp[4] = {
    pow(instance->wrapper_cc[CC_AMP_A]/127.0, 3),
    pow(instance->wrapper_cc[CC_AMP_B]/127.0, 3),
    pow(instance->wrapper_cc[CC_AMP_C]/127.0, 3),
    pow(instance->wrapper_cc[CC_AMP_D]/127.0, 3),
  };
  FOR(o, NUM_OSCS) osc[v][o] += velocity * amp[o];
}

static void handle_midi_event(struct instance *instance, const jack_midi_event_t* event) {
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

static void generate_audio(struct instance *instance, int start_frame, int end_frame) {
  const double coarse[NUM_OSCS] = {
    instance->wrapper_cc[CC_FREQ_A] - 64,
    instance->wrapper_cc[CC_FREQ_B] - 64,
    instance->wrapper_cc[CC_FREQ_C] - 64,
    instance->wrapper_cc[CC_FREQ_D] - 64,
  };
  double freq[NUM_VOICES][NUM_OSCS];
  double fine = (instance->wrapper_cc[CC_FINE_TUNE] - 64) / 64.0;
  FOR(v, NUM_VOICES) {
    FOR(o, NUM_OSCS) {
      freq[v][o] = instance->freq[key[v]] * pow(2.0, (coarse[o] + fine) / 12.0);
    }
  }
  const double amp_decay[NUM_OSCS] = {
    pow(instance->wrapper_cc[CC_DECAY_A] / 128.0, 5)*100,
    pow(instance->wrapper_cc[CC_DECAY_B] / 128.0, 5)*100,
    pow(instance->wrapper_cc[CC_DECAY_C] / 128.0, 5)*100,
    pow(instance->wrapper_cc[CC_DECAY_D] / 128.0, 5)*100,
  };
  const double amp_release[NUM_OSCS] = {
    pow(instance->wrapper_cc[CC_RELEASE_A] / 128.0, 5)*100,
    pow(instance->wrapper_cc[CC_RELEASE_B] / 128.0, 5)*100,
    pow(instance->wrapper_cc[CC_RELEASE_C] / 128.0, 5)*100,
    pow(instance->wrapper_cc[CC_RELEASE_D] / 128.0, 5)*100,
  };
  double complex step[NUM_VOICES][NUM_OSCS];
  FOR(v, NUM_VOICES) {
    FOR(o, NUM_OSCS) {
      step[v][o] = cexp(-dt * freq[v][o] * (2.0 * 3.151492i + (gate[v] ? amp_decay[o] : amp_release[o])));
    }
  }
  for (int i = start_frame; i < end_frame; ++i) {
    FOR(v, NUM_VOICES) {
      FOR(o, NUM_OSCS) {
	osc[v][o] *= step[v][o];
      }
    }
    double sum = 0;
    FOR(v, NUM_VOICES) {
      FOR(o, NUM_OSCS) {
	sum += cimag(osc[v][o]);
      }
    }
    audio_out_buf[i] = sum * 0.0625;
  }
}

void plugin_process(struct instance *instance, int nframes) {
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

void plugin_init(struct instance *instance, double sample_rate) {
  dt = 1.0 / sample_rate;
  wrapper_add_midi_input(instance, "midi in", &midi_in_buf);
  wrapper_add_audio_output(instance, "audio out", &audio_out_buf);
  wrapper_add_cc(instance, CC_FREQ_A, "Freq A", "freq_a", 64);
  wrapper_add_cc(instance, CC_FREQ_B, "Freq B", "freq_b", 64);
  wrapper_add_cc(instance, CC_FREQ_C, "Freq C", "freq_c", 64);
  wrapper_add_cc(instance, CC_FREQ_D, "Freq D", "freq_d", 64);
  wrapper_add_cc(instance, CC_AMP_A, "Amp A", "amp_a", 64);
  wrapper_add_cc(instance, CC_AMP_B, "Amp B", "amp_b", 64);
  wrapper_add_cc(instance, CC_AMP_C, "Amp C", "amp_c", 64);
  wrapper_add_cc(instance, CC_AMP_D, "Amp D", "amp_d", 64);
  wrapper_add_cc(instance, CC_DECAY_A, "Decay A", "decay_a", 64);
  wrapper_add_cc(instance, CC_DECAY_B, "Decay B", "decay_b", 64);
  wrapper_add_cc(instance, CC_DECAY_C, "Decay C", "decay_c", 64);
  wrapper_add_cc(instance, CC_DECAY_D, "Decay D", "decay_d", 64);
  wrapper_add_cc(instance, CC_RELEASE_A, "Release A", "release_a", 64);
  wrapper_add_cc(instance, CC_RELEASE_B, "Release B", "release_b", 64);
  wrapper_add_cc(instance, CC_RELEASE_C, "Release C", "release_c", 64);
  wrapper_add_cc(instance, CC_RELEASE_D, "Release D", "release_d", 64);
  wrapper_add_cc(instance, CC_FINE_TUNE, "Fine Tune", "fine_tune", 64);
}

void plugin_destroy(struct instance* instance) {
}

#include <jack/midiport.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "Kick2";
const char* plugin_persistence_name = "mjack_kick2";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif 

#define FOR(var,limit) for(int var = 0; var < limit; ++var)

static void* midi_in_buf;
static float* audio_out_buf;
static char key_is_pressed[128];
static int key_count_pressed;
static int current_key = -1;
static int current_velocity;

// constants
static double dt;

// settings

#define CC_DECAY 80
#define CC_FREQ 81
#define CC_DROP 82

// dsp control values
static double pos;
static double vel;
static bool trigger;

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
    current_velocity = velocity;
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
  double freq = 440.0 * pow(2.0, (instance->wrapper_cc[CC_FREQ] - 67 + current_key - 67) / 12.0);
  double k = 3.14159 * 2 * freq * dt;
  for (int i = start_frame; i < end_frame; ++i) {
    // TODO apply BLT
    double acc = -pos - pos*pos*pos*pow(instance->wrapper_cc[CC_DROP]/128.0, 2)*128  - vel * pow((1 + instance->wrapper_cc[CC_DECAY]) / 128.0, 2) * 16;
    if (trigger) {
      acc += current_velocity / 128.0 / k;
      trigger = false;
    }
    vel += acc * k;
    pos += vel * k;
    audio_out_buf[i] = pos * 4;
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
  wrapper_add_cc(instance, CC_FREQ, "Freq", "freq", 64);
  wrapper_add_cc(instance, CC_DECAY, "Decay", "decay", 64);
  wrapper_add_cc(instance, CC_DROP, "Drop", "drop", 64);
}

void plugin_destroy(struct instance* instance) {
}

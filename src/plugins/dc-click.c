#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include "../wrappers/wrapper.h"

const char* plugin_name = "dc_click";
const char* plugin_persistence_name = "mjack_dc_click";

#define FOR(var,limit) for(int var = 0; var < limit; ++var)

static void* midi_in_buf;
static float* audio_out_buf;
static char key_is_pressed[128];

// constants
static double dt;

static double state;

// settings

#define CC_DECAY 80

static void handle_midi_note_off(int key, int velocity) {
  if (key_is_pressed[key]) {
    key_is_pressed[key] = 0;
  }
}

static void handle_midi_note_on(int key, int velocity) {
  if (!key_is_pressed[key]) {
    key_is_pressed[key] = 1;
    state = velocity / 256.0;
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

static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  double w = 2.0 * 3.141592 * 10.0 * pow(10000.0/10.0, instance->wrapper_cc[CC_DECAY]/127.0);
  double decay_coeff = exp(-dt * w);
  for (int i = start_frame; i < end_frame; ++i) {
    audio_out_buf[i] = state;
    state *= decay_coeff;
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
  dt = 1.0 / sample_rate;
  wrapper_add_midi_input(instance, "midi in", &midi_in_buf);
  wrapper_add_audio_output(instance, "audio out", &audio_out_buf);
  wrapper_add_cc(instance, CC_DECAY, "Decay", "decay", 64);
}

void plugin_destroy(struct instance* instance) {
}


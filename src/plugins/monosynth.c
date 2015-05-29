#include <jack/midiport.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

#include "../wrappers/wrapper.h"
#include "../dsp/osc.h"

const char* plugin_name = "MonoSynth";
const char* plugin_persistence_name = "mjack_monosynth";

#define KNOBS							\
  X(CC_VOLUME, 80, "Volume", 127)				\
  X(CC_CUTOFF, 81, "Cutoff", 64)				\
  X(CC_RESONANCE, 82, "Resonance", 64)				\
  X(CC_TRACKING, 83, "Tracking", 0)				\
  X(CC_SIDE_HP_CUTOFF, 84, "Side Cutoff", 64)			\

enum {
#define X(name,index,label,value) name = index,
  KNOBS
#undef X
};

#define FOR(var,limit) for(int var = 0; var < limit; ++var)

static void* midi_in_buf;
static float* audio_out_buf;
static char key_is_pressed[128];
static int key_count_pressed;
static int current_key = -1;

// constants
static double dt;

// dsp control values
static double gain;

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

static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  double freq = 440.0 * pow(2.0, (current_key - 69) / 12.0);


  double wc = 440.0 * pow(2.0,
			  (instance->wrapper_cc[CC_CUTOFF] - 69 + 24) / 12.0 +
			  (current_key - 69) / 12.0 * (instance->wrapper_cc[CC_TRACKING] / 127.0)
			  ) * 2 * 3.141592;
  double kc = -expm1(-wc * dt);


  double wfb = 440.0 * pow(2.0,
			   (instance->wrapper_cc[CC_SIDE_HP_CUTOFF] - 69) / 12.0
			   ) * 2 * 3.141592;
  double kfb = -expm1(-wfb * dt);
			    
  double reso = instance->wrapper_cc[CC_RESONANCE] / 128.0;

  printf("%g %g %g %g\n", kc, kfb, reso, kfb);


  double volume = instance->wrapper_cc[CC_VOLUME] * instance->wrapper_cc[CC_VOLUME] / (127.0 * 127.0) * 0.25;
  for (int i = start_frame; i < end_frame; ++i) {
    static double phase;
    phase += freq * dt;
    while (phase > 1.0) { phase -= 1.0; }
    double osc = 2 * phase - 1;

    osc *= 0.25;

    double tmp = osc;

    // filter
    {
      static double pole[4];
      static double fb_pole;
      static double zero;

      double fb = 0.5 * (pole[3] + zero);
      zero = pole[3];

      fb -= (fb_pole += (fb - fb_pole) * kfb);

      tmp -= reso * tanh(fb*6);
      FOR(j, 4) {
	tmp = (pole[j] += (tmp - pole[j]) * kc);
      }
    }

    osc *= 4;
    
    audio_out_buf[i] = tmp * gain * volume;
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

#define X(name, index, label, value) wrapper_add_cc(instance, index, label, #name, value);
  KNOBS
#undef X
}

void plugin_destroy(struct instance* instance) {
}

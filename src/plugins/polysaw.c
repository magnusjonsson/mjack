#include <jack/midiport.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../wrappers/wrapper.h"
#include "../dsp/osc.h"
#include "../dsp/svf.h"
#include "../dsp/ladder.h"
#include "../dsp/ms20-filter.h"
#include "../dsp/lfo.h"
#include "../dsp/adsr-env.h"
#include "../dsp/simple-env.h"

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
#define CC_VCF1_CUTOFF 81
#define CC_VCF1_RESONANCE 82
#define CC_VCF1_CLIP_LEVEL 83
#define CC_VCF1_TRACKING 84

#define CC_VCF2_CUTOFF 85
#define CC_VCF2_RESONANCE 86
#define CC_VCF2_CLIP_LEVEL 87
#define CC_VCF2_TRACKING 88

#define CC_VCF_PRE_GAIN 89
#define CC_OCTAVE 90
#define CC_FIFTH 91

#define CC_VCA_ATTACK 92
#define CC_VCA_DECAY 93
#define CC_VCA_SUSTAIN 94
#define CC_VCA_RELEASE 95

#define CC_VCF1_ATTACK 96
#define CC_VCF1_DECAY 97
#define CC_VCF1_SUSTAIN 98
#define CC_VCF1_RELEASE 99

#define CC_VCF2_ATTACK 100
#define CC_VCF2_DECAY 101
#define CC_VCF2_SUSTAIN 102
#define CC_VCF2_RELEASE 103

#define CC_DRIFT 104
#define CC_LFO_DELAY 105
#define CC_LFO_FREQ 106
#define CC_OSC_LFO 107
#define CC_PW 108
#define CC_PW_LFO 109

// dsp control values
static double gain[NUM_VOICES];
static double osc_freq[NUM_VOICES];
static double env_freq[NUM_VOICES];

// dsp state
static struct osc osc[NUM_VOICES];

//static struct svf svf[NUM_VOICES];
static struct ladder ladder[NUM_VOICES];
//static struct ms20_filter ms20_filter_1[NUM_VOICES];
//static struct ms20_filter ms20_filter_2[NUM_VOICES];

static struct adsr_env vca_env[NUM_VOICES];
static struct adsr_env vcf1_env[NUM_VOICES];
static struct adsr_env vcf2_env[NUM_VOICES];

struct lfo lfo[NUM_VOICES];

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

const struct {
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

const double ji_ratio[12] = {
  1.0,
  17.0/16.0,
  9.0/8.0,
  19.0/16.0,
  5.0/4.0,
  21.0/16.0,
  11.0/8.0,
  3.0/2.0,
  13.0/8.0,
  27.0/16.0,
  7.0/4.0,
  15.0/8.0,
};

static double key_cents(struct instance *instance, int key) {
  int note = key%12;
  int oct = key/12 - 5;
  if (instance->wrapper_cc[CC_OCTAVE] == 0 && instance->wrapper_cc[CC_FIFTH] == 0) {
    return 1200.0 * oct + 1200.0 * log(ji_ratio[note]) / log(2);
  } else {
    double OCTAVE_CENTS = 1200.0 + 0.1 * (instance->wrapper_cc[CC_OCTAVE]-64);
    double FIFTH_CENTS = 700.0 + 0.1 * (instance->wrapper_cc[CC_FIFTH]-64);
    return (OCTAVE_CENTS * (oct + note_vector[note].octaves - note_vector[9].octaves) + FIFTH_CENTS * (note_vector[note].fifths - note_vector[9].fifths));
  }
}

static int next_voice;

static void handle_midi_note_on(struct instance* instance, int key, int velocity) {
  if (!key_is_pressed[key]) {
    key_is_pressed[key] = 1;
    FOR(v, NUM_VOICES) {
      if (gain[v] > 0 && (current_key[v] == key - 1 || current_key[v] == key + 1)) {
	osc_freq[v] = 440.0 * pow(2.0, (key_cents(instance, key) + key_cents(instance, current_key[v])) * 0.5 / 1200.0);
	current_key[v] = key;
	return;
      }
    }
    FOR(count, NUM_VOICES) {
      int v = next_voice++;
      if (next_voice >= NUM_VOICES) next_voice = 0;
      if (gain[v] == 0.0) {
	current_key[v] = key;

	osc_freq[v] = 440.0 * pow(2.0, key_cents(instance, key) / 1200.0);
	env_freq[v] = osc_freq[v] * 1.0;
	adsr_env_trigger(&vca_env[v]);
	adsr_env_trigger(&vcf1_env[v]);
	adsr_env_trigger(&vcf2_env[v]);
	lfo_trigger(&lfo[v]);
	gain[v] = velocity / 127.0;
	return;
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
  case 0xB0:
    fprintf(stderr, "Received CC %i -> %i\n", event->buffer[1], event->buffer[2]);
    instance->wrapper_cc[event->buffer[1]] = event->buffer[2];
    break;
  }
}

static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  for(int i = start_frame; i < end_frame; ++i) {
    audio_out_buf[i] = 0.0;
  }
  double vca_attack  = pow(instance->wrapper_cc[CC_VCA_ATTACK] / 128.0, 4) * 10000;
  double vca_decay   = pow(instance->wrapper_cc[CC_VCA_DECAY] / 128.0, 4) * 10000;
  double vca_sustain = pow(instance->wrapper_cc[CC_VCA_SUSTAIN] / 128.0, 2);
  double vca_release = pow(instance->wrapper_cc[CC_VCA_RELEASE] / 128.0, 4) * 10000;
  double vcf1_attack  = pow(instance->wrapper_cc[CC_VCF1_ATTACK] / 128.0, 4) * 10000;
  double vcf1_decay   = pow(instance->wrapper_cc[CC_VCF1_DECAY] / 128.0, 4) * 10000;
  double vcf1_sustain = pow(instance->wrapper_cc[CC_VCF1_SUSTAIN] / 128.0, 2);
  double vcf1_release = pow(instance->wrapper_cc[CC_VCF1_RELEASE] / 128.0, 4) * 10000;

  //double vcf2_attack  = pow(instance->wrapper_cc[CC_VCF2_ATTACK] / 128.0, 4) * 10000;
  //double vcf2_decay   = pow(instance->wrapper_cc[CC_VCF2_DECAY] / 128.0, 4) * 10000;
  //double vcf2_sustain = pow(instance->wrapper_cc[CC_VCF2_SUSTAIN] / 128.0, 2);
  //double vcf2_release = pow(instance->wrapper_cc[CC_VCF2_RELEASE] / 128.0, 4) * 10000;

  double vcf_pregain = pow(instance->wrapper_cc[CC_VCF_PRE_GAIN] / 64.0, 2);
  double reso_1 = instance->wrapper_cc[CC_VCF1_RESONANCE] / 127.0;
  //double reso_2 = instance->wrapper_cc[CC_VCF2_RESONANCE] / 127.0;
  //double svf1_q = 1.0 - instance->wrapper_cc[CC_VCF1_RESONANCE] / 127.0;
  //double svf2_q = 1.0 - instance->wrapper_cc[CC_VCF2_RESONANCE] / 127.0;
  double vcf1_clip_level = pow((instance->wrapper_cc[CC_VCF1_CLIP_LEVEL] + 1) / 64.0, 2);
  //double vcf2_clip_level = pow((instance->wrapper_cc[CC_VCF2_CLIP_LEVEL] + 1) / 64.0, 2);
  double volume = instance->wrapper_cc[CC_VOLUME] * instance->wrapper_cc[CC_VOLUME] / (127.0 * 127.0);
  double drift = pow(instance->wrapper_cc[CC_DRIFT], 2) * sqrt(dt);
  double lfo_delay = 10 * pow(instance->wrapper_cc[CC_LFO_DELAY] / 128.0, 2);
  double lfo_freq = 20 * pow(instance->wrapper_cc[CC_LFO_FREQ] / 128.0, 2);
  double osc_lfo = 0.05 * pow(instance->wrapper_cc[CC_OSC_LFO] / 128.0, 2);
  double pw = 0.5 * instance->wrapper_cc[CC_PW] / 128.0;
  double pw_lfo = 0.5 * instance->wrapper_cc[CC_PW_LFO] / 128.0;
  FOR(v, NUM_VOICES) {
    double svf1_freq = 440.0 * pow(osc_freq[v]/110.0, instance->wrapper_cc[CC_VCF1_TRACKING] / 127.0) * pow(2.0, (instance->wrapper_cc[CC_VCF1_CUTOFF] - 69 + 24 + 4) / 12.0);
    //double svf2_freq = 440.0 * pow(osc_freq[v]/110.0, instance->wrapper_cc[CC_VCF2_TRACKING] / 127.0) * pow(2.0, (instance->wrapper_cc[CC_VCF2_CUTOFF] - 69 + 24 + 4) / 12.0);
    for (int i = start_frame; i < end_frame; ++i) {
      double lfo_out = lfo_tick(&lfo[v], dt, lfo_delay, lfo_freq);
      double osc_out = osc_tick(&osc[v], dt, osc_freq[v] * (1 + osc_lfo * lfo_out) + (rand()*2.0/RAND_MAX - 1.0) * drift);
      osc_out = osc_out > pw + pw_lfo * lfo_out ? 0.6 : -0.6;
      double vcf1_env_out = adsr_env_tick(&vcf1_env[v], dt, vcf1_attack, vcf1_decay, vcf1_sustain, vcf1_release, gain[v], 2);
      //double vcf2_env_out = adsr_env_tick(&vcf2_env[v], dt, vcf2_attack, vcf2_decay, vcf2_sustain, vcf2_release, gain[v], 2);
      double f1 = svf1_freq * vcf1_env_out;
      //double f2 = svf2_freq / vcf2_env_out;
      double svf_out_1 = ladder_tick(&ladder[v], dt, f1, 6 * reso_1, vcf1_clip_level, osc_out * vcf_pregain);
      //double svf_out_1 = svf_tick_nonlinear(&svf[v], dt, f1, svf1_q, osc_out * vcf_pregain);
      //double svf_out_1 = ms20_filter_tick_lp(&ms20_filter_1[v], dt, f1, reso_1, vcf1_clip_level, osc_out * vcf_pregain);
      //double svf_out_2 = ms20_filter_tick_lp(&ms20_filter_2[v], dt, f2, reso_2, vcf2_clip_level, svf_out_1);
      double vca_env_out = adsr_env_tick(&vca_env[v], dt, vca_attack, vca_decay, vca_sustain, vca_release, gain[v], 0);
      audio_out_buf[i] += svf_out_1 * vca_env_out * volume;
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
  wrapper_add_cc(instance, CC_VCA_ATTACK, "VCA Attack", "vca_attack", 64);
  wrapper_add_cc(instance, CC_VCA_DECAY, "VCA Decay", "vca_decay", 64);
  wrapper_add_cc(instance, CC_VCA_SUSTAIN, "VCA Sustain", "vca_sustain", 64);
  wrapper_add_cc(instance, CC_VCA_RELEASE, "VCA Release", "vca_release", 64);

  wrapper_add_cc(instance, CC_VCF1_CUTOFF, "VCF1 Cutoff", "vcf1_cutoff", 64);
  wrapper_add_cc(instance, CC_VCF1_RESONANCE, "VCF1 Resonance", "vcf1_resonance", 64);
  wrapper_add_cc(instance, CC_VCF1_CLIP_LEVEL, "VCF1 Clip Level", "vcf1_clip_level", 64);
  wrapper_add_cc(instance, CC_VCF1_TRACKING, "VCF1 Tracking", "vcf1_tracking", 0);
  wrapper_add_cc(instance, CC_VCF2_CUTOFF, "VCF2 Cutoff", "vcf2_cutoff", 64);
  wrapper_add_cc(instance, CC_VCF2_RESONANCE, "VCF2 Resonance", "vcf2_resonance", 64);
  wrapper_add_cc(instance, CC_VCF2_CLIP_LEVEL, "VCF2 Clip Level", "vcf2_clip_level", 64);
  wrapper_add_cc(instance, CC_VCF2_TRACKING, "VCF2 Tracking", "vcf2_tracking", 0);
  wrapper_add_cc(instance, CC_VCF1_ATTACK, "VCF1 Attack", "vcf1_attack", 64);
  wrapper_add_cc(instance, CC_VCF1_DECAY, "VCF1 Decay", "vcf1_decay", 64);
  wrapper_add_cc(instance, CC_VCF1_SUSTAIN, "VCF1 Sustain", "vcf1_sustain", 64);
  wrapper_add_cc(instance, CC_VCF1_RELEASE, "VCF1 Release", "vcf1_release", 64);
  wrapper_add_cc(instance, CC_VCF2_ATTACK, "VCF2 Attack", "vcf2_attack", 64);
  wrapper_add_cc(instance, CC_VCF2_DECAY, "VCF2 Decay", "vcf2_decay", 64);
  wrapper_add_cc(instance, CC_VCF2_SUSTAIN, "VCF2 Sustain", "vcf2_sustain", 64);
  wrapper_add_cc(instance, CC_VCF2_RELEASE, "VCF2 Release", "vcf2_release", 64);
  wrapper_add_cc(instance, CC_OCTAVE, "Octave", "octave", 64);
  wrapper_add_cc(instance, CC_FIFTH, "Fifth", "fifth", 64);
  wrapper_add_cc(instance, CC_DRIFT, "Drift", "drift", 64);
  wrapper_add_cc(instance, CC_LFO_DELAY, "Lfo Delay", "lfo_delay", 64);
  wrapper_add_cc(instance, CC_LFO_FREQ, "Lfo Freq", "lfo_freq", 64);
  wrapper_add_cc(instance, CC_OSC_LFO, "OSC LFO", "osc_lfo", 64);
  wrapper_add_cc(instance, CC_PW, "PW", "pw", 64);
  wrapper_add_cc(instance, CC_PW_LFO, "PW LFO", "pw_lfo", 64);
}

void plugin_destroy(struct instance* instance) {
}

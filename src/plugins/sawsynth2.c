#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

#include "../wrappers/wrapper.h"

const char* plugin_name = "SawSynth2";
const char* plugin_persistence_name = "mjack_sawsynth2";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif 

#define FOR(var,limit) for(int var = 0; var < limit; ++var)

static void *midi_in_buf;
static float *audio_out_buf;
static char key_is_pressed[128];
static int key_count_pressed;
static int current_key = -1;

// constants
static double dt;

// settings

#define KNOBS \
  X(CC_VOLUME, 7, "Volume") \
  X(CC_WAVEFORM, 120, "Waveform") \
  X(CC_CUTOFF, 77, "Cutoff") \
  X(CC_RESONANCE, 71, "Resonance") \
  X(CC_CUTOFF_KBD, 80, "Cutoff Kbd") \
  X(CC_AMP_DECAY, 121, "AMP Decay") \
  X(CC_VCF_DECAY, 122, "VCF Decay") \
  X(CC_CLICK, 123, "Click") \
  X(CC_LFO_RATE, 124, "LFO Rate") \
  X(CC_PITCH_LFO, 125, "Pitch LFO") \
  X(CC_CUTOFF_LFO, 126, "Cutoff LFO") \

enum {
#define X(name,value,_) name = value,
  KNOBS
#undef X
};


void plugin_init(struct instance* instance, double sample_rate) {
  dt = 1.0 / sample_rate;
  wrapper_add_midi_input(instance, "midi in", &midi_in_buf);
  wrapper_add_audio_output(instance, "audio out", &audio_out_buf);
#define X(name, value, label) wrapper_add_cc(instance, value, label, #name, 64);
  KNOBS
#undef X
}

void plugin_destroy(struct instance* instance) {
}



// control state
static double amp_env;
static double vcf_env;
static double osc_freq;
static double lfo_phase;


struct lpf {
  double re;
  double im;
};

void lpf_tick(struct lpf *lpf, double pole_re, double pole_im, double dt, double input_A) {
  if (pole_im <= 0 || pole_re >= 0) {
    return;
  }
  double phi_re = exp(pole_re * dt) * cos(pole_im * dt);
  double phi_im = exp(pole_re * dt) * sin(pole_im * dt);

  double re = lpf->re;
  double im = lpf->im;

  double input_re = input_A;
  double input_im = input_A * pole_re / pole_im;
  
  re -= input_re; // part 1 of integral
  im -= input_im;
  
  double new_re = re * phi_re - im * phi_im;
  double new_im = re * phi_im + im * phi_re;

  new_re += input_re; // part 2 of intergral
  new_im += input_im;
  
  lpf->re = new_re;
  lpf->im = new_im;
}
/*
  double input_re = input_A * pole_im / sqrt(pole_re * pole_re + pole_im * pole_im);
  double input_im = input_A * pole_re / sqrt(pole_re * pole_re + pole_im * pole_im);
  
  lpf->re = new_re + input_re + (1 - phi_re) * input_re + phi_im * input_im;
  lpf->im = new_im + input_im      - phi_im  * input_re + phi_re * input_im;
}
*/

double lpf_sample(struct lpf *lpf) {
  return lpf->re;
}

// dsp state
struct osc_state {
  double phase;
  struct lpf lpf;
};

static struct osc_state osc_state;
static double smoothed_osc_freq;
static double smoothed_amp;
static double smoothed_vcf_env;

static void handle_midi_note_off(int key, int velocity) {
  if (key_is_pressed[key]) {
    key_is_pressed[key] = 0;
    --key_count_pressed;
    if (key_count_pressed == 0) {
      amp_env = 0.0;
    }
  }
}

static void handle_midi_note_on(struct instance *instance, int key, int velocity) {
  if (!key_is_pressed[key]) {
    key_is_pressed[key] = 1;
    ++key_count_pressed;
    current_key = key;
    osc_freq = instance->freq[key];
    amp_env = velocity / 127.0;
    vcf_env = velocity / 127.0;
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

static inline double tick_osc_square(struct osc_state* osc_state, double freq, double re, double im, double threshold) {
  double remaining = dt;
  while (remaining > 0) {
    double osc = osc_state->phase >= 0 ? 1 : -1;
    double step = remaining;
    double new_phase = osc_state->phase + freq * remaining;
    if (osc_state->phase >= 0 && new_phase > 0.5) {
      step = (0.5 - osc_state->phase) / freq;
      new_phase = -0.5;
    } else if (osc_state->phase < threshold && new_phase > threshold) {
      step = (threshold - osc_state->phase) / freq;
      new_phase = threshold;
    } else {
      // initial assumption correct
    }
    lpf_tick(&osc_state->lpf, re, im, step, osc);
    remaining -= step;
    osc_state->phase = new_phase;
  }
  return lpf_sample(&osc_state->lpf);
}

static double tick_osc_saw(struct osc_state* osc, double freq, double re, double im) {
  double remaining = dt;
  while (remaining > 0) {
    double step = remaining;
    double new_phase = osc->phase + freq * remaining;
    if (new_phase >= 0.5) {
      step = (0.5 - osc->phase) / freq;
      new_phase = -0.5;
    } else {
      // initial assumption correct
    }
    lpf_tick(&osc->lpf, re, im, step, 2 * osc->phase + dt * freq);
    remaining -= step;
    osc->phase = new_phase;
  }
  return lpf_sample(&osc->lpf);
}

static double tick_osc(struct osc_state* osc, int waveform, double freq, double re, double im) {
  if (waveform < 32) {
    return tick_osc_square(osc, freq, re, im, 0);
  }
  else if (waveform < 64) {
    return tick_osc_square(osc, freq, re, im, -0.25);
  }
  else {
    return tick_osc_saw(osc, freq, re, im);
  }
}


static void generate_audio(struct instance* instance, int start_frame, int end_frame) {
  double cutoff = 2 * M_PI * 440.0 * pow(2.0,
					 (instance->wrapper_cc[CC_CUTOFF] - 69 + 24 + 4) / 12.0 +
					 (current_key - 45) / 12.0 * (instance->wrapper_cc[CC_CUTOFF_KBD] / 127.0)
					 );
  double reso = (0.5 + instance->wrapper_cc[CC_RESONANCE]) / 128.0;
  double volume = pow(instance->wrapper_cc[CC_VOLUME]/ 127.0, 0.5) * 0.25;
  double lfo_rate = 20 * pow(instance->wrapper_cc[CC_LFO_RATE]/127.0, 3);
  double amp_decay_rate = 100 * pow(instance->wrapper_cc[CC_AMP_DECAY] / 127.0, 4);
  double vcf_decay_rate = 10000 * pow(instance->wrapper_cc[CC_VCF_DECAY] / 127.0, 4);
  double pitch_lfo = 0.5 * pow(instance->wrapper_cc[CC_PITCH_LFO]/127.0, 4);
  double cutoff_lfo = 0.5 * pow(instance->wrapper_cc[CC_CUTOFF_LFO]/127.0, 4);
  double smoothing_coeff = fmin(0.5, 1000 * pow((1 + instance->wrapper_cc[CC_CLICK]) / 128.0, 3) * dt);

  int waveform = instance->wrapper_cc[CC_WAVEFORM];
  
  for (int i = start_frame; i < end_frame; ++i) {
    // envelope
    vcf_env -= vcf_env * fmin(0.5, vcf_env * vcf_env * vcf_decay_rate * dt);
    amp_env -= amp_env * fmin(0.5, amp_decay_rate * dt);

    // lfo
    lfo_phase += lfo_rate * dt;
    if (lfo_phase >= 1.0) { lfo_phase -= 1.0; }
    double lfo = -1 + fabs(lfo_phase * 4 - 2);

    
    // declick
    smoothed_vcf_env += (vcf_env - smoothed_vcf_env) * smoothing_coeff;
    smoothed_osc_freq += (osc_freq - smoothed_osc_freq) * smoothing_coeff;
    smoothed_amp += (volume * amp_env - smoothed_amp) * smoothing_coeff;

    // control values
    double re = - cutoff * smoothed_vcf_env * (1 + lfo * cutoff_lfo) * sqrt(1 - reso);
    double im =   cutoff * smoothed_vcf_env * (1 + lfo * cutoff_lfo) * sqrt(reso);

    // audio path
    audio_out_buf[i] = smoothed_amp * tick_osc(&osc_state, waveform, smoothed_osc_freq * (1 + lfo * pitch_lfo), re, im);
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

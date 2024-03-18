#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"
#include "lv2/lv2plug.in/ns/ext/patch/patch.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define SYNTH_URI "urn:magnusjonsson:mjack:lv2:harmonic_synth"

#include "../util/lv2utils.h"
#include "../util/uris.h"
#include "../../tuning/scala.h"

#define FOR(i, n) for(int i = 0; i < n; i++)

#define ANTIALIAS_NUM_POLES 2
#define NUM_OSC 4
#define NUM_LFO 2

#define PARAMS			\
  X(atk)			\
  X(dcy)			\
  X(sus)			\
  X(rls)			\
  X(tune1)			\
  X(tune2)			\
  X(tune3)			\
  X(tune4)			\
  X(fine1)			\
  X(fine2)			\
  X(fine3)			\
  X(fine4)			\
  X(gain1)			\
  X(gain2)			\
  X(gain3)			\
  X(gain4)			\
  X(glide_lin)			\
  X(glide_exp)			\
  X(spd)			\
  X(spd2)			\
  X(dep)			\
  X(dep2)			\
  X(lpf)                        \
  X(lpf_trk)                    \
  X(lpf_env)                    \
  X(res)                        \
  X(amp_env)                    \
  X(waveform)                   \

enum port {
  port_control         = 0,
  port_notify          = 1,
  port_out_left        = 2,
  port_out_right       = 3,
#define X(name) port_##name,
  PARAMS
#undef X
};

struct params {
#define X(name) float const *name;
  PARAMS
#undef X
};

struct voice {
  double lfosaw[NUM_LFO];
  double lfosqr[NUM_LFO];

  double saw[NUM_OSC];

  double target_freq;
  double freq;

  //double antialias[2][ANTIALIAS_NUM_POLES];

  bool held;
  double gain;

  double body_dcy;
  double body_atk;
};

static void voice_init(struct voice *v) {
  FOR(i, NUM_LFO) {
    v->lfosaw[i] = 0;
  }

  v->target_freq = 220;
  v->freq = 220;

  FOR(i, NUM_OSC) {
    v->saw[i] = 0;
  }
  FOR(i, NUM_LFO) {
    v->lfosqr[i] = 1;
  }

  v->held = false;

  v->body_dcy = 0;
  v->body_atk = 0;
}

static double square(double x) {
  return x * x;
}

static double cube(double x) {
  return x * x * x;
}

static double rate_shape(double param) {
  return 10000 * square(param);
}

static void voice_tick(struct voice *v, struct params const *p, double dt, float *out_left, float *out_right) {
  // ENV

  if (v->held) {
    v->body_dcy += (square(*p->sus) - v->body_dcy) * fmin(0.5, dt * rate_shape(*p->dcy));
  } else {
    v->body_dcy += (1e-20           - v->body_dcy) * fmin(0.5, dt * rate_shape(*p->rls));
  }
  v->body_atk += (v->body_dcy - v->body_atk) * fmin(0.5, dt * rate_shape(*p->atk));

  double body_env = v->body_atk;

  // LFO


  double spd[NUM_LFO] = {
    square(*p->spd),
    square(*p->spd2),
  };
  double dep[NUM_LFO] = {
    square(*p->dep),
    square(*p->dep2),
  };
  double lfo[NUM_LFO];
  FOR(i, NUM_LFO) {
    double lforate = dt * spd[i] * 100;
    v->lfosaw[i] += lforate;
    while(v->lfosaw[i] <= -0.5) { v->lfosaw[i] += 1.0; v->lfosqr[i] = -v->lfosqr[i]; }
    while(v->lfosaw[i] > 0.5) { v->lfosaw[i] -= 1.0; v->lfosqr[i] = -v->lfosqr[i]; }
    double lfotri = 2 * v->lfosaw[i] * v->lfosqr[i];
    lfo[i] = lfotri;
  };

  // OSC

  double tune[NUM_OSC] = {
    *p->tune1,
    *p->tune2,
    *p->tune3,
    *p->tune4,
  };
  double fine[NUM_OSC] = {
    *p->fine1,
    *p->fine2,
    *p->fine3,
    *p->fine4,
  };
  double gain[NUM_OSC] = {
    *p->gain1,
    *p->gain2,
    *p->gain3,
    *p->gain4,
  };

  double r_mod = 1.0;
  FOR(i, NUM_LFO) {
    r_mod += dep[i] * lfo[i];
  }

  v->freq += (v->target_freq - v->freq) * fmin(0.5, dt * (rate_shape(*p->glide_lin) + square(*p->glide_exp) * v->freq));

  double freq = fmax(1e-6, v->freq * r_mod);

  // Gains

  double rate[NUM_OSC];
  FOR(i, NUM_OSC) {
    rate[i] = freq * fmax(1e-6, tune[i] * (1 + fine[i]));
  }
  double w = dt * 100000 * cube(*p->lpf) * pow(freq/220, *p->lpf_trk) * pow(2*body_env, *p->lpf_env);

  double g_overall = pow(2*body_env, *p->amp_env);
  
  double sum = 0;

  FOR(i, NUM_OSC) {
    double travel = dt * rate[i];
    double phase = v->saw[i];
    phase += travel;
    while(phase >= 1) {
      phase -= 1.0;
    }
    v->saw[i] = phase;
    double g = gain[i] * g_overall;

    switch ((int) *p->waveform) {
    case 0: // saw
      {
	double c = cos(phase*2*3.141592) * exp(-16*travel/w);
	double s = sin(phase*2*3.141592) * exp(-16*travel/w);
	sum += atan(s/(1+c)) * g;
      }
      break;
    case 1: // square 1
      {
	double s = sin(phase*2*3.141592) * 0.1 * w / travel;
	sum += s / sqrt(1 + s * s) * g;
      }
      break;
    case 2: // square 2
      {
	double s = sin(phase*2*3.141592);
	sum += atan(s * 0.1 * w / travel) * g;
      }
      break;
    case 3: // "square" impulse train with filter
      {
	double k_abs = -2*3.141592 * fmax(1e-6, w/travel);
	double k_cos = cos(0.5*3.141592 * fmax(0.005, fmin(0.99, *p->res)));
	double k_sin = sin(0.5*3.141592 * fmax(0.005, fmin(0.99, *p->res)));
	double k_re = k_abs * k_cos;
	double k_im = k_abs * k_sin;
	double p2 = phase + 0.5;
	if (p2 >= 1) {
	  p2 -= 1;
	}
	double a = exp(phase * k_re) * cos(phase * k_im) - exp(p2 * k_re) * cos(p2 * k_im);
	double b = exp(phase * k_re) * sin(phase * k_im) - exp(p2 * k_re) * sin(p2 * k_im);
	double A = exp(k_re) * cos(k_im);
	double B = exp(k_re) * sin(k_im);

	//a / (1 - A) - (A - 1)/k / (1 - A) = -1/k
	
	sum += (a * B + b*(1-A)) / ((1-A)*(1-A) + B*B) * g * k_cos;
      }
      break;
    case 4: // "saw" impulse train with filter
      {
	double k_abs = -2*3.141592 * fmax(1e-6, w/travel);
	double k_cos = cos(0.5*3.141592 * fmax(0.005, fmin(0.99, *p->res)));
	double k_sin = sin(0.5*3.141592 * fmax(0.005, fmin(0.99, *p->res)));
	double k_re = k_abs * k_cos;
	double k_im = k_abs * k_sin;
	double a = exp(phase * k_re) * cos(phase * k_im);
	double b = exp(phase * k_re) * sin(phase * k_im);
	double A = exp(k_re) * cos(k_im);
	double B = exp(k_re) * sin(k_im);

	//a / (1 - A) - (A - 1)/k / (1 - A) = -1/k

	double bias_im = k_im / (k_re * k_re + k_im * k_im);
	double pole_im = (a * B + b*(1-A)) / ((1-A)*(1-A) + B*B);

	sum += (pole_im - bias_im) * g * k_cos;
      }
      break;
    }
  }
  *out_left  = sum;
  *out_right = sum;
}

static void voice_handle_note_on(struct voice *v, float freq, int velo) {
  fprintf(stderr, "note on! target_freq: %f  freq: %f\n", freq, v->freq);
  v->target_freq = freq;
  v->held = true;
  v->body_dcy = 1.0 / 128.0 * velo;
  FOR(i, NUM_LFO) {
    fprintf(stderr, "lfosaw[%i]=%f\n", i, v->lfosaw[i]);
    fprintf(stderr, "lfosqr[%i]=%f\n", i, v->lfosqr[i]);
  }
}

static void voice_handle_note_off(struct voice *v) {
  v->held = false;
  v->gain = 0;
}

struct synth {
  double dt;

  LV2_URID_Map *map;
  LV2_URID_Unmap *unmap;

  LV2_Atom_Forge forge;
  LV2_Atom_Forge_Frame notify_frame;
  struct uris uris;

  char scala_file[PATH_MAX];

  const LV2_Atom_Sequence *control;
  const LV2_Atom_Sequence *notify;
  float *out[2];

  struct params params;

  int current_note;
  float freq[128];

  struct voice voice;
};

static LV2_Handle instantiate(const LV2_Descriptor *descriptor,
			      double sample_rate,
			      const char *bundle_path,
			      const LV2_Feature *const *host_features) {
  fprintf(stderr, "Instantiating\n");
  struct synth *self;
  {
    self = calloc(1, sizeof(struct synth));
    self->dt = 1.0 / sample_rate;
  }

  fprintf(stderr, "Getting mappers\n");
  {
    self->map = get_host_feature(host_features, LV2_URID__map);
    if (!self->map) { fprintf(stderr, "Could not get URID map host feature\n"); goto err; }
    self->unmap = get_host_feature(host_features, LV2_URID__unmap);
    if (!self->unmap) { fprintf(stderr, "Could not get URID unmap host feature\n"); goto err; }
  }

  fprintf(stderr, "Mapping uris\n");
  {
    uris_init(&self->uris, self->map);
  }

  fprintf(stderr, "Initializing forge\n");
  {
    lv2_atom_forge_init(&self->forge, self->map);
  }

  fprintf(stderr, "Initializing frequency table\n");
  {
    for (int i = 0; i < 128; i++) {
      self->freq[i] = 440.0 * pow(2.0, (i - 60.0) / 12.0);
    }
  }
  fprintf(stderr, "Initializing voice\n");
  voice_init(&self->voice);

  fprintf(stderr, "Done\n");
  return self;
 err:
  fprintf(stderr, "Could not initialize\n");
  free(self);
  return NULL;
}

static void connect_port(LV2_Handle instance, uint32_t port, void *data) {
  struct synth *self = instance;
  switch (port) {
  case port_control: self->control = data; break;
  case port_notify: self->notify = data; break;
  case port_out_left: self->out[0] = data; break;
  case port_out_right: self->out[1] = data; break;
#define X(name) case port_##name: self->params.name = data; break;
    PARAMS
#undef X
  }
}

static void activate(LV2_Handle instance) {
  fprintf(stderr, "activate()\n");
}

static void handle_note_on(struct synth *self, int note, int velocity) {
  self->current_note = note;
  voice_handle_note_on(&self->voice, self->freq[note], velocity);
}

static void handle_note_off(struct synth *self, int note) {
  if (note == self->current_note) {
    voice_handle_note_off(&self->voice);
  }
}

static void handle_midi(struct synth *self, const uint8_t *msg) {
  switch (msg[0]) {
  case LV2_MIDI_MSG_NOTE_ON:
    if (msg[2] > 0) { handle_note_on(self, msg[1], msg[2]); }
    if (msg[2] == 0) { handle_note_off(self, msg[1]); }
    break;
  case LV2_MIDI_MSG_NOTE_OFF:
    handle_note_off(self, msg[1]);
    break;
  default:
    break;
  }
}

static void run_audio(struct synth *self, uint32_t offset, uint32_t end) {
  if (offset == end) { return; }
  for (int i = offset; i < end; i++) {
    voice_tick(&self->voice, &self->params, self->dt, &self->out[0][i], &self->out[1][i]);
  }
}

static bool set_scala_file(struct synth *self, const char *filename) {
  if (strlen(filename) < PATH_MAX) {
    strncpy(self->scala_file, filename, PATH_MAX-1);
    self->scala_file[PATH_MAX-1] = '\0';
    // TODO do this in worker thread
    float cents[128];
    float freq[128];
    if (!load_scala_file(filename, cents, freq)) {
      fprintf(stderr, "Failed to load scala file %s\n", filename);
      return false;
    }
    for (int i = 0; i < 128; i++) {
      self->freq[i] = freq[i];
    }
    return true;
  } else {
    fprintf(stderr, "Scala file path too long!\n");
    return false;
  }
}

static void handle_patch_set(struct synth *self, const LV2_Atom_Object *obj, int64_t frames) {
  fprintf(stderr, "Got patch set\n");
  const char *filename = read_set_scala_file(obj, &self->uris);
  if (filename) {
    fprintf(stderr, "scala file name: %s\n", filename);
    set_scala_file(self, filename);
  } else {
    fprintf(stderr, "Could not get scala filename from patch_set");
  }
}

static void handle_patch_get(struct synth *self, const LV2_Atom_Object *obj, int64_t frames) {
  fprintf(stderr, "Got patch get\n");
  lv2_atom_forge_frame_time(&self->forge, frames);
  write_set_scala_file(&self->forge, &self->uris, self->scala_file);
}

static void handle_atom_object(struct synth *self, const LV2_Atom_Object *obj, int64_t frames) {
  if (obj->body.otype == self->uris.patch_set) {
    handle_patch_set(self, obj, frames);
  }
  else if (obj->body.otype == self->uris.patch_get) {
    handle_patch_get(self, obj, frames);
  }
  else {
    fprintf(stderr, "Unknown atom object body type: %s", self->unmap->unmap(self->unmap->handle, obj->body.otype));
  }
}

static void run(LV2_Handle handle, uint32_t nframes) {
  struct synth *self = handle;

  const uint32_t notify_capacity = self->notify->atom.size;
  lv2_atom_forge_set_buffer(&self->forge,
			    (uint8_t *)self->notify,
			    notify_capacity);
  lv2_atom_forge_sequence_head(&self->forge, &self->notify_frame, 0);

  uint32_t offset = 0;
  LV2_ATOM_SEQUENCE_FOREACH(self->control, ev) {
    run_audio(self, offset, ev->time.frames);
    if (ev->body.type == self->uris.midi_event) {
      const uint8_t *const msg = (const uint8_t *)(ev + 1);
      handle_midi(self, msg);
    }
    else if (ev->body.type == self->uris.atom_object) {
      const LV2_Atom_Object *obj = (const void *) &ev->body;
      handle_atom_object(self, obj, offset);
    }
    else {
      fprintf(stderr, "Unknown event type: %s", self->unmap->unmap(self->unmap->handle, ev->body.type));
    }
    offset = ev->time.frames;
  }
  run_audio(self, offset, nframes);
}

static void deactivate(LV2_Handle instance) {
  fprintf(stderr, "deactivate\n");
}

static void cleanup(LV2_Handle instance) {
  fprintf(stderr, "cleanup\n");
  free(instance);
}

static LV2_State_Status save(LV2_Handle instance,
			     LV2_State_Store_Function store_fn,
			     LV2_State_Handle handle,
			     uint32_t flags,
			     const LV2_Feature *const *features)
{
  fprintf(stderr, "save\n");
  struct synth *self = instance;
  LV2_State_Map_Path *map_path = get_host_feature(features, LV2_STATE__mapPath);
  if (!map_path) {
    fprintf(stderr, "no map path host feature\n");
    return LV2_STATE_ERR_NO_FEATURE;
  }
  char *apath = map_path->abstract_path(map_path->handle, self->scala_file);
  if (!apath) {
    fprintf(stderr, "abstract_path() returned NULL\n");
    return LV2_STATE_ERR_UNKNOWN;
  }
  LV2_State_Status status =
    store_fn(handle,
	     self->uris.scala_file,
	     apath,
	     strlen(apath) + 1,
	     self->uris.atom_path,
	     LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
  free(apath);
  if (status != LV2_STATE_SUCCESS) {
    fprintf(stderr, "store_fn failed\n");
  }
  return status;
}

static LV2_State_Status restore(LV2_Handle instance,
				LV2_State_Retrieve_Function retrieve_fn,
				LV2_State_Handle handle,
				uint32_t flags,
				const LV2_Feature * const *features) {
  fprintf(stderr, "restore\n");
  struct synth *self = instance;
  size_t size;
  LV2_URID type;
  uint32_t valflags;
  const char *scala_file =
    retrieve_fn(handle,
		self->uris.scala_file,
		&size, &type, &valflags);
  if (!scala_file) {
    fprintf(stderr, "Failed to retrieve scala file state\n");
    return LV2_STATE_ERR_UNKNOWN;
  }
  if (size == 0 || size > PATH_MAX) {
    fprintf(stderr, "Bad scala file path length %zi\n", size);
    return LV2_STATE_ERR_UNKNOWN;
  }
  if (scala_file[size-1] != '\0') {
    fprintf(stderr, "Scala file not null terminated!\n");
    return LV2_STATE_ERR_UNKNOWN;
  }
  fprintf(stderr, "Scala file: %s\n", scala_file);
  if (!set_scala_file(self, scala_file)) {
    return LV2_STATE_ERR_UNKNOWN;
  }
  return LV2_STATE_SUCCESS;
}

static const void *extension_data(const char *uri) {
  fprintf(stderr, "extension_data\n");
  if (!strcmp(uri, LV2_STATE__interface)) {
    static const LV2_State_Interface state = { save, restore };
    return &state;
  } else {
    fprintf(stderr, "Unknown extension data requested: %s\n", uri);
  }
  return NULL;
}

static const LV2_Descriptor descriptor = {
  SYNTH_URI,
  instantiate,
  connect_port,
  activate,
  run,
  deactivate,
  cleanup,
  extension_data,
};

LV2_SYMBOL_EXPORT const LV2_Descriptor *lv2_descriptor(uint32_t index) {
  switch (index) {
  case 0:  return &descriptor;
  default: return NULL;
  }
}

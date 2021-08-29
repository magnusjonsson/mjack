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

#define NUM_VOICES 128
#define FOR(i, n) for(int i = 0; i < n; i++)

#define PARAMS			\
  X(3,saw_perc)			\
  X(4,par_perc)			\
  X(5,sqr_perc)			\
  X(6,tri_perc)			\
  X(7,saw)			\
  X(8,par)			\
  X(9,sqr)			\
  X(10,tri)			\
  X(11,atk)			\
  X(12,rls)			\
  X(13,spd)			\
  X(14,dep)			\

enum port {
  port_control         = 0,
  port_notify          = 1,
  port_out             = 2,
#define X(i, name) port_##name = i,
  PARAMS
#undef X
};

struct params {
#define X(i, name) float const *name;
  PARAMS
#undef X
};

struct voice {
  double lfosaw;
  double lfosqr;

  double freq;

  double saw;
  double sqr;
  
  double gain;

  double perc;
  double env;
};

static void voice_init(struct voice *v, double freq) {
  v->lfosaw = 0;
  v->lfosqr = -0.5;

  v->freq = freq;

  v->saw = 0;
  v->sqr = -0.5;

  v->perc = 0;
  v->gain = 0;

  v->perc = 0;
  v->env = 0;
}

static double voice_tick(struct voice *v, struct params const *p, double dt) {
  double lforate = dt * *p->spd;
  v->lfosaw += lforate;
  while(v->lfosaw > 0.5) { v->lfosaw -= 1.0; v->lfosqr =-v->lfosqr; }
  double lfosaw = v->lfosaw;
  double lfosqr = v->lfosqr;
  double lfotri = 2 * lfosaw * lfosqr;
  
  double rate = dt * (v->freq * (1.0 + *p->dep * lfotri));
  v->saw += rate;
  while(v->saw > 0.5) { v->saw -= 1.0; v->sqr = -v->sqr; }
  double saw = v->saw;
  double par = 4 * saw * saw - (1.0 / 3.0);
  double sqr = v->sqr;
  double tri = 2 * saw * sqr;

  double tgt = v->gain + 1e-20;
  double spd = v->gain > 0 ? *p->atk : *p->rls;
  double k = fmin(0.5, spd * dt);

  v->perc += (1e-20 - v->perc) * k;
  v->env += (tgt - v->env) * k;

  double out_env = 0;
  out_env += *p->saw * saw;
  out_env += *p->par * par;
  out_env += *p->sqr * sqr;
  out_env += *p->tri * tri;
  double out_perc = 0;
  out_perc += *p->saw_perc * saw;
  out_perc += *p->par_perc * par;
  out_perc += *p->sqr_perc * sqr;
  out_perc += *p->tri_perc * tri;
  return 0.15 * (out_env * v->env + out_perc * v->perc);
}

static void voice_handle_note_on(struct voice *v, int velo) {
  v->gain = 1.0 / 128.0 * velo;
  fprintf(stderr, "v->perc before %lf\n", v->perc);
  v->perc += 1.0 / 128.0 * velo;
  fprintf(stderr, "v->perc after %lf\n", v->perc);
}

static void voice_handle_note_off(struct voice *v) {
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
  float *out;

  struct params params;

  struct voice voice[128];
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

  fprintf(stderr, "Initializing voices\n");
  {
    for (int i = 0; i < 128; i++) {
      double freq = 440.0 * pow(2.0, (i - 60.0) / 12.0);
      voice_init(&self->voice[i], freq);
    }
  }

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
  case port_out: self->out = data; break;
#define X(i, name) case port_##name: self->params.name = data; break;
    PARAMS
#undef X
  }
}

static void activate(LV2_Handle instance) {
  fprintf(stderr, "activate()\n");
}

static void handle_note_on(struct synth *self, int note, int velocity) {
  voice_handle_note_on(&self->voice[note], velocity);
}

static void handle_note_off(struct synth *self, int note) {
  voice_handle_note_off(&self->voice[note]);
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
  for (int v = 0; v < NUM_VOICES; v++) {
    for (int i = offset; i < end; i++) {
      self->out[i] += voice_tick(&self->voice[v], &self->params, self->dt);
    }
  }
}

static bool set_scala_file(struct synth *self, const char *filename) {
  if (strlen(filename) < PATH_MAX) {
    strncpy(self->scala_file, filename, PATH_MAX);
    // TODO do this in worker thread
    float cents[128];
    float freq[128];
    if (!load_scala_file(filename, cents, freq)) {
      fprintf(stderr, "Failed to load scala file %s\n", filename);
      return false;
    }
    for (int i = 0; i < 128; i++) {
      self->voice[i].freq = freq[i];
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

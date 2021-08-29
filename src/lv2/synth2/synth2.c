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

#define SYNTH_URI "urn:magnusjonsson:mjack:lv2:synth2"

#include "../util/lv2utils.h"
#include "../util/uris.h"
#include "../../tuning/scala.h"

#define NUM_VOICES 128

#define PARAMS					\
  X(3, tone)					\
  X(4, decay)					\
  X(5, decay_env)				\
  X(6, pitch)					\
  X(7, pitch_env)				\
  X(8, release)					\
  X(9, release_env)				\

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

struct mass {
  double c;
  double s;
};

static double mass_tick(struct mass *m, struct params const *p, double input, double w0, double damping) {
  double c = m->c;
  double s = m->s;
  double amp2 = c * c + s * s;
  if (amp2 < 1e-12 && fabs(input < 1e-6)) {
    return 0.0;
  }
  double w = w0 * *p->pitch * (1.0 + *p->pitch_env * amp2);
  double d = w0 * (*p->decay * (1.0 + *p->decay_env * amp2) +
		   *p->release * (1.0 + *p->release_env * amp2) * damping);
  if (w > 3.1592) {
    w = 3.15192;
  }
  double rotor_c = cos(w) * exp(-d);
  double rotor_s = sin(w) * exp(-d);
  double new_c = rotor_c * c - rotor_s * s + input;
  double new_s = rotor_s * c + rotor_c * s;
  m->c = new_c;
  m->s = new_s;
  return new_c;
}

struct voice {
  double freq;
  double stimulus;
  double damping;
  struct mass mass;
};

static void voice_init(struct voice *v, double freq) {
  v->freq = freq;
}

static double voice_tick(struct voice *v, struct params const *p, double dt) {
  double w0 = 2 * 3.141592 * v->freq * dt;
  double x = v->stimulus * fmin(1.0, *p->tone * dt);
  v->stimulus -= x;
  return mass_tick(&v->mass, p, x, w0, v->damping);
}

static void voice_handle_note_on(struct voice *v, int velo) {
  v->stimulus += 0.5 / 128.0 * velo;
  v->damping = 0;
}

static void voice_handle_note_off(struct voice *v) {
  v->damping = 1;
}

struct synth {
  double dt;

  LV2_URID_Map *map;
  LV2_URID_Unmap *unmap;

  LV2_Atom_Forge forge;
  LV2_Atom_Forge_Frame notify_frame;
  struct uris uris;

  const LV2_Atom_Sequence *control;
  const LV2_Atom_Sequence *notify;
  float *out;
  struct params params;
  char scala_file[PATH_MAX];

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
    fprintf(stderr, "Could not get scala filename from patch_Set");
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

static const void *extension_data(const char *uri) {
  fprintf(stderr, "extension_data\n");
  fprintf(stderr, "Unknown extension data requested: %s\n", uri);
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

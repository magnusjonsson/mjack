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

#include "lv2utils.h"
#include "uris.h"

#include "cv.h"
#include "midi_to_cv.h"
#include "osc.h"

enum port {
  PORT_CONTROL = 0,
  PORT_NOTIFY = 1,
  PORT_OUT = 2,
  PORT_VIBRATO_1_RATE = 3,
  PORT_VIBRATO_1_DEPTH = 4,
  PORT_VIBRATO_2_RATE = 5,
  PORT_VIBRATO_2_DEPTH = 6,
  PORT_VIBRATO_3_RATE = 7,
  PORT_VIBRATO_3_DEPTH = 8,
  PORT_OCTAVE = 9,
  PORT_WAVEFORM = 10,
  PORT_SPEED = 11,
};

#define NUM_VIBRATOS 3

struct synth {
  double dt;

  LV2_URID_Map *map;
  LV2_URID_Unmap *unmap;

  LV2_Atom_Forge forge;
  LV2_Atom_Forge_Frame notify_frame;

  struct synth_uris uris;
  
  const LV2_Atom_Sequence *control;
  const LV2_Atom_Sequence *notify;
  float *out;

  const float *vibrato_rate[NUM_VIBRATOS];
  const float *vibrato_depth[NUM_VIBRATOS];
  const float *octave;
  const float *waveform;
  const float *speed;

  char scala_file[PATH_MAX];

  double amp;

  struct osc vibrato_lfo[NUM_VIBRATOS];

  struct midi_to_cv midi_to_cv;
  struct osc osc;
};

static LV2_Handle instantiate(const LV2_Descriptor *descriptor,
			      double sample_rate,
			      const char *bundle_path,
			      const LV2_Feature *const *host_features) {
  fprintf(stderr, "instantiate\n");
  struct synth *self = calloc(1, sizeof(struct synth));
  self->dt = 1.0 / sample_rate;
  self->map = get_host_feature(host_features, LV2_URID__map);
  self->unmap = get_host_feature(host_features, LV2_URID__unmap);
  if (!self->map) {
    fprintf(stderr, "Could not get URID map host feature\n");
    goto err;
  }
  if (!self->unmap) {
    fprintf(stderr, "Could not get URID unmap host feature\n");
    goto err;
  }
  fprintf(stderr, "Mapping uris\n");
  synth_uris_init(&self->uris, self->map);

  fprintf(stderr, "initializing forge\n");
  lv2_atom_forge_init(&self->forge, self->map);

  midi_to_cv_init(&self->midi_to_cv);

  fprintf(stderr, "instantiate done\n");
  return self;
 err:
  fprintf(stderr, "instantiate error\n");
  free(self);
  return NULL;
}

static void connect_port(LV2_Handle instance, uint32_t port, void *data) {
  struct synth *self = instance;
  switch (port) {
  case PORT_CONTROL: self->control = data; break;
  case PORT_NOTIFY: self->notify = data; break;
  case PORT_OUT: self->out = data; break;
  case PORT_VIBRATO_1_RATE: self->vibrato_rate[0] = data; break;
  case PORT_VIBRATO_1_DEPTH: self->vibrato_depth[0] = data; break;
  case PORT_VIBRATO_2_RATE: self->vibrato_rate[1] = data; break;
  case PORT_VIBRATO_2_DEPTH: self->vibrato_depth[1] = data; break;
  case PORT_VIBRATO_3_RATE: self->vibrato_rate[2] = data; break;
  case PORT_VIBRATO_3_DEPTH: self->vibrato_depth[2] = data; break;
  case PORT_OCTAVE: self->octave = data; break;
  case PORT_WAVEFORM: self->waveform = data; break;
  case PORT_SPEED: self->speed = data; break;
  }
}

static void activate(LV2_Handle instance) {
  fprintf(stderr, "activate\n");
}

static void handle_midi(struct synth *self, const uint8_t *msg) {
  fprintf(stderr, "Got midi event, %x %x %x!\n", msg[0], msg[1], msg[2]);
  switch (lv2_midi_message_type(msg)) {
  case LV2_MIDI_MSG_NOTE_ON: // fall through
  case LV2_MIDI_MSG_NOTE_OFF:
    midi_to_cv_handle_midi(&self->midi_to_cv, msg);
    break;
  case LV2_MIDI_MSG_PGM_CHANGE:
    break;
  default:
    break;
  }
}

static double vibrato_tick(struct synth *self) {
  double sum = 0;
  for(int j = 0; j < NUM_VIBRATOS; j++) {
    if (!self->vibrato_depth[j]) continue;
    if (!self->vibrato_rate[j]) continue;
    double depth = *self->vibrato_depth[j];
    double rate = *self->vibrato_rate[j] * self->dt;
    sum += depth * osc_tick_sin(&self->vibrato_lfo[j], rate);
  }
  return sum;
}

static void run_audio(struct synth *self, uint32_t offset, uint32_t end) {
  if (offset == end) { return; }
  double freq_multiplier = pow(2, *self->octave);
  double cv_freq = self->midi_to_cv.cv.freq * freq_multiplier * self->dt;
  double speed = -expm1(-self->dt * *self->speed);
  int waveform = (int) *self->waveform;
  for (int i = offset; i < end; i++) {
    double vibrato = vibrato_tick(self);
    double freq = cv_freq * (1 + vibrato);
    double osc = 0;
    switch (waveform) {
    case 0: osc = osc_tick_saw_box(&self->osc, freq); break;
    case 1: osc = osc_tick_duty_box(&self->osc, freq, 1.0/2.0); break;
    case 2: osc = osc_tick_duty_box(&self->osc, freq, 1.0/3.0); break;
    case 3: osc = osc_tick_duty_box(&self->osc, freq, 1.0/4.0); break;
    case 4: osc = osc_tick_duty_box(&self->osc, freq, 1.0/5.0); break;
    case 5: osc = osc_tick_duty_box(&self->osc, freq, 2.0/5.0); break;
    case 6: osc = osc_tick_duty_box(&self->osc, freq, 1.0/6.0); break;
    case 7: osc = osc_tick_duty_box(&self->osc, freq, 1.0/7.0); break;
    case 8: osc = osc_tick_duty_box(&self->osc, freq, 2.0/7.0); break;
    case 9: osc = osc_tick_duty_box(&self->osc, freq, 3.0/7.0); break;
    case 10: osc = osc_tick_duty_box(&self->osc, freq, 1.0/8.0); break;
    case 11: osc = osc_tick_duty_box(&self->osc, freq, 1.0/9.0); break;
    case 12: osc = osc_tick_duty_box(&self->osc, freq, 2.0/9.0); break;
    case 13: osc = osc_tick_duty_box(&self->osc, freq, 4.0/9.0); break;
    case 14: osc = osc_tick_duty_box(&self->osc, freq, 1.0/10.0); break;
    case 15: osc = osc_tick_duty_box(&self->osc, freq, 3.0/10.0); break;
    case 16: osc = osc_tick_duty_box(&self->osc, freq, 1.0/11.0); break;
    case 17: osc = osc_tick_duty_box(&self->osc, freq, 2.0/11.0); break;
    case 18: osc = osc_tick_duty_box(&self->osc, freq, 3.0/11.0); break;
    case 19: osc = osc_tick_duty_box(&self->osc, freq, 4.0/11.0); break;
    case 20: osc = osc_tick_duty_box(&self->osc, freq, 5.0/11.0); break;
    case 21: osc = osc_tick_duty_box(&self->osc, freq, 1.0/12.0); break;
    case 22: osc = osc_tick_duty_box(&self->osc, freq, 5.0/12.0); break;
    case 23: osc = osc_tick_sin(&self->osc, freq); break;
    }
    self->amp += (self->midi_to_cv.cv.gate - self->amp) * speed;
    self->out[i] = self->amp * osc;
  }
  // TODO add allpass filters
  // TODO add more waveforms
}

static bool set_scala_file(struct synth *self, const char *filename) {
  if (strlen(filename) < PATH_MAX) {
    strncpy(self->scala_file, filename, PATH_MAX);
    // TODO do this in worker thread
    midi_to_cv_load_scala_file(&self->midi_to_cv, self->scala_file);
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

static LV2_State_Status save(LV2_Handle instance,
			     LV2_State_Store_Function store_fn,
			     LV2_State_Handle handle,
			     uint32_t flags,
			     const LV2_Feature *const *features)
{
  struct synth *self = instance;
  LV2_State_Map_Path *map_path = get_host_feature(features, LV2_STATE__mapPath);
  if (!map_path) {
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
  return status;
}

static LV2_State_Status restore(LV2_Handle instance,
				LV2_State_Retrieve_Function retrieve_fn,
				LV2_State_Handle handle,
				uint32_t flags,
				const LV2_Feature * const *features) {
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
  static const LV2_State_Interface state = { save, restore };
  if (!strcmp(uri, LV2_STATE__interface)) {
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

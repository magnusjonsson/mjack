#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define SYNTH_URI "urn:magnusjonsson:mjack:lv2:synth"

#include "lv2utils.h"

enum port_index {
  PORT_CONTROL = 0,
  PORT_OUT = 1,
};

struct osc {
  double phase;
};

static double osc_tick_saw(struct osc *self, double freq) {
  double out = self->phase;
  self->phase += freq;
  while (self->phase >= 0.5) {
    self->phase -= 1.0;
  }
  return out;
}

struct cv {
  double freq;
  double gate;
};

struct midi_to_cv {
  uint8_t current_note;
  struct cv cv;
};

static void midi_to_cv_handle_midi(struct midi_to_cv *self, const uint8_t *msg) {
  switch (lv2_midi_message_type(msg)) {
  case LV2_MIDI_MSG_NOTE_ON:
    fprintf(stderr, "Note on %i %i\n", msg[1], msg[2]);
    if (msg[2] != 0) {
      self->current_note = msg[1];
      self->cv.freq = 440 * pow(2.0, (msg[1] - 69) / 12.0);
      self->cv.gate = msg[2] / 127.0;
      break;
    }
    // fall through
  case LV2_MIDI_MSG_NOTE_OFF:
    fprintf(stderr, "Note off %i %i\n", msg[1], msg[2]);
    if (msg[1] == self->current_note) {
      self->cv.gate = 0;
    }
    break;
  default:
    break;
  }
}

struct synth {
  LV2_URID midi_event_type;
  const LV2_Atom_Sequence *control;
  float *out;
  double dt;
  struct midi_to_cv midi_to_cv;
  struct osc osc;
};

static LV2_Handle instantiate(const LV2_Descriptor *descriptor,
			      double sample_rate,
			      const char *bundle_path,
			      const LV2_Feature *const *host_features) {
  fprintf(stderr, "%s called!", __func__);
  struct synth *self = calloc(1, sizeof(struct synth));
  self->dt = 1.0 / sample_rate;
  LV2_URID_Map *urid_map = get_host_feature(host_features, LV2_URID__map);
  if (!urid_map) {
    fprintf(stderr, "Could not get URID map host feature\n");
    goto err;
  }
  self->midi_event_type = urid_map->map(urid_map->handle, LV2_MIDI__MidiEvent);
  return self;
 err:
  free(self);
  return NULL;
}

static void connect_port(LV2_Handle instance, uint32_t port, void *data) {
  struct synth *self = instance;
  switch (port) {
  case PORT_CONTROL: self->control = data; break;
  case PORT_OUT: self->out = data; break;
  }
}

static void activate(LV2_Handle instance) {
}

static void handle_midi(struct synth *self, const uint8_t *msg) {
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

static void run_audio(struct synth *self, uint32_t offset, uint32_t end) {
  if (offset == end) { return; }
  for (int i = offset; i < end; i++) {
    self->out[i] = osc_tick_saw(&self->osc, self->midi_to_cv.cv.freq * self->dt) * self->midi_to_cv.cv.gate;
  }
}

static void run(LV2_Handle handle, uint32_t nframes) {
  struct synth *self = handle;
  uint32_t offset = 0;
  LV2_ATOM_SEQUENCE_FOREACH(self->control, ev) {
    run_audio(self, offset, ev->time.frames);
    if (ev->body.type == self->midi_event_type) {
      const uint8_t *const msg = (const uint8_t *)(ev + 1);
      handle_midi(self, msg);
    }
    offset = ev->time.frames;
  }
  run_audio(self, offset, nframes);
}

static void deactivate(LV2_Handle instance) {
}

static void cleanup(LV2_Handle instance) {
  free(instance);
}

static const void *extension_data(const char *uri) {
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
  fprintf(stderr, "%s called!", __func__);
  switch (index) {
  case 0:  return &descriptor;
  default: return NULL;
  }
}

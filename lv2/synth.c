#include <math.h>
#include <stdlib.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/midi/midi.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define SYNTH_URI "http://mjack/synth/"

enum port_index {
  PORT_CONTROL = 0,
  PORT_OUT = 1,
};

struct synth {
  const LV2_Atom_Sequence *control;

  LV2_URID midi_event_type;
  
  float *out;
};

static void *get_host_feature(const LV2_Feature *const *host_features, const char *uri) {
  for (int i = 0; host_features[i]; i++) {
    if (strcmp(host_features[i]->URI, uri) == 0) {
      return host_features[i]->data;
    }
  }
  return NULL;
}

static LV2_Handle instantiate(const LV2_Descriptor *descriptor,
			      double sample_rate,
			      const char *bundle_path,
			      const LV2_Feature *const *host_features) {
  struct synth *self = calloc(1, sizeof(struct synth));
  LV2_URID_Map *urid_map = get_host_feature(host_features, LV2_URID__map);
  if (!urid_map) { goto error; }
  self->midi_event_type = urid_map->map(urid_map->handle, LV2_MIDI__MidiEvent);
 error:
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
  case LV2_MIDI_MSG_NOTE_ON:
    break;
  case LV2_MIDI_MSG_NOTE_OFF:
    break;
  case LV2_MIDI_MSG_PGM_CHANGE:
    break;
  default:
    break;
  }
}

static void run_audio(struct synth *self, uint32_t offset, uint32_t len) {
  if (len == 0) { return; }
  for (int i = offset; i < len; i++) {
    self->out[i] = 0.0f;
  }
}

static void run(LV2_Handle handle, uint32_t nframes) {
  struct synth *self = handle;
  uint32_t offset = 0;
  LV2_ATOM_SEQUENCE_FOREACH(self->control, ev) {
    run_audio(self, offset, ev->time.frames - offset);
    if (ev->body.type == self->midi_event_type) {
      const uint8_t *const msg = (const uint8_t *)(ev + 1);
      handle_midi(self, msg);
    }
    offset = ev->time.frames;
  }
  run_audio(self, offset, nframes - offset);
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
  switch (index) {
  case 0:  return &descriptor;
  default: return NULL;
  }
}

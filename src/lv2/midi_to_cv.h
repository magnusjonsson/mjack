#include "../tuning/scala.h"

struct midi_to_cv {
  uint8_t note;
  uint8_t velocity;
  bool trigger;
  bool retrigger;
  float freq[128];
};

static void midi_to_cv_init(struct midi_to_cv *self) {
  fprintf(stderr, "%s\n", __func__);
  for (int i = 0; i < 128; i++) {
    self->freq[i] = 440 * pow(2.0, (i - 69) / 12.0);
  }
}

static void midi_to_cv_handle_midi(struct midi_to_cv *self, const uint8_t *msg) {
  switch (lv2_midi_message_type(msg)) {
  case LV2_MIDI_MSG_NOTE_ON:
    if (msg[2] != 0) {
      if (self->velocity == 0) {
	self->trigger = true;
      } else {
	self->retrigger = true;
      }
      self->note = msg[1];
      self->velocity = msg[2];
      break;
    }
    // fall through
  case LV2_MIDI_MSG_NOTE_OFF:
    if (msg[1] == self->note) {
      self->velocity = 0;
    }
    break;
  default:
    break;
  }
}

static struct cv midi_to_cv_get_cv(struct midi_to_cv *self) {
  struct cv cv = {
    .freq = self->freq[self->note],
    .gate = self->velocity / 127.0,
    .trigger = self->trigger,
    .retrigger = self->retrigger,
  };
  self->trigger = false;
  self->retrigger = false;
  return cv;
}

static bool midi_to_cv_load_scala_file(struct midi_to_cv *self, const char *filename) {
  fprintf(stderr, "%s\n", __func__);
  float cents[128];
  if (!load_scala_file(filename, cents, self->freq)) {
    fprintf(stderr, "Failed to load scala file %s\n", filename);
    return false;
  }
  return true;
}

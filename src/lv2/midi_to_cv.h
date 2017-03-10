#include "../tuning/scala.h"

struct midi_to_cv {
  struct cv cv;
  uint8_t current_note;
  float freq[128];
};

static void midi_to_cv_init(struct midi_to_cv *self) {
  fprintf(stderr, "%s\n", __func__);
  for (int i = 0; i < 128; i++) {
    self->freq[i] = 440 * pow(2.0, (i - 69) / 12.0);
  }
}

static void midi_to_cv_handle_midi(struct midi_to_cv *self, const uint8_t *msg) {
  fprintf(stderr, "%s\n", __func__);
  switch (lv2_midi_message_type(msg)) {
  case LV2_MIDI_MSG_NOTE_ON:
    fprintf(stderr, "Note on %i %i\n", msg[1], msg[2]);
    if (msg[2] != 0) {
      self->current_note = msg[1];
      self->cv.freq = self->freq[self->current_note];
      fprintf(stderr, "%f\n", self->cv.freq);
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


static void midi_to_cv_load_scala_file(struct midi_to_cv *self, const char *filename) {
  fprintf(stderr, "%s\n", __func__);
  float cents[128];
  load_scala_file(filename, cents, self->freq);
  self->cv.freq = self->freq[self->current_note];
}

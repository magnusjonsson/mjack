static const struct {
  int octaves;
  int fifths;
} meantone_note_vector[12] = {
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

// return value relative to A-440.
static inline double meantone_cents(int key, double octave_cents, double fifth_cents) {
  int note = key%12;
  int oct = key/12 - 5;
  return
    octave_cents * (oct + meantone_note_vector[note].octaves - meantone_note_vector[9].octaves) +
    fifth_cents  * (      meantone_note_vector[note].fifths  - meantone_note_vector[9].fifths);
}

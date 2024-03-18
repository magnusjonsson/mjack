// based on schematic on http://www.synthforum.nl/forums/showthread.php?t=138512&highlight=ms+20+filter
// which is particularly clear, with added nonlinearity in feedback path as found in other ms20 filter clone schematics

struct ms20_filter {
  double z1;
  double z2;
};

// arbitrarily made up, asymmetric to introduce even harmonics
static inline double ms20_filter_shape_gain(double x) {
  return 1 / sqrt(1.0 + x * x);
}

static inline double ms20_filter_shape(double clip_level, double x) {
  return x * ms20_filter_shape_gain(x / clip_level);
}

static inline double ms20_filter_tick(struct ms20_filter *s, double dt, double freq, double reso, double clip_level, double lp_input, double hp_input) {
  double k = fmin(1.0, dt * 2 * 3.141592 * freq);
  double z2_prime = s->z2 + hp_input;
  double z1_prime = s->z1 + ms20_filter_shape(clip_level, 2 * z2_prime * reso);
  s->z1 += (lp_input - z1_prime) * k;
  z1_prime += (lp_input - z1_prime) * k;
  s->z2 += (z1_prime - z2_prime) * k;
  z2_prime += (z1_prime - z2_prime) * k;
  return z2_prime;
}

static inline double ms20_filter_tick_lp(struct ms20_filter *s, double dt, double freq, double reso, double clip_level, double input) {
  return ms20_filter_tick(s, dt, freq, reso, clip_level, input, 0.0);
}

static inline double ms20_filter_tick_hp(struct ms20_filter *s, double dt, double freq, double reso, double clip_level, double input) {
  return ms20_filter_tick(s, dt, freq, reso, clip_level, 0.0, input);
}

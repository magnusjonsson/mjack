struct ms20_filter {
  double z1;
  double z2;
};

static inline double ms20_filter_shape_gain(double x) {
  return 1 / sqrt(1.0 + 0.66 * x + x * x);
}

static inline double ms20_filter_shape(double clip_level, double x) {
  return x * ms20_filter_shape_gain(x / clip_level);
}

static inline double ms20_filter_tick(struct ms20_filter *s, double dt, double freq, double reso, double clip_level, double input) {
  double k = fmin(1.0, dt * 2 * 3.141592 * freq);
  double z1_prime = s->z1 + ms20_filter_shape(clip_level, 2 * s->z2 * reso);
  s->z1 += (input - z1_prime) * k;
  s->z2 += (z1_prime - s->z2) * k;
  return s->z2;
}

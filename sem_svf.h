struct sem_svf {
  double z1;
  double z2;
};

static inline double sem_svf_shape_gain(double x) {
  return 1 / sqrt(1.0 + 0.66 * x + x * x);
}

static inline double sem_svf_shape(double clip_level, double x) {
  return x * sem_svf_shape_gain(x / clip_level);
}

static inline double sem_svf_tick(struct sem_svf *s, double dt, double freq, double reso, double clip_level, double input) {
  double k = fmin(1.0, dt * 2 * 3.141592 * freq);
  double g = 1 - sem_svf_shape_gain(s->z1 / clip_level);
  s->z1 += k * sem_svf_shape(1, input - s->z2 - s->z1 * (reso + g));
  s->z2 += k * sem_svf_shape(1, s->z1);
  return s->z2;
}

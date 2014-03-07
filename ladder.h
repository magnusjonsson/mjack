struct ladder {
  double z0, z1, z2, z3;
};

static inline double ladder_shape(double x) {
  return tanh(x);
}

static inline double ladder_tick(struct ladder *ladder, double dt, double f0, double f1, double f2, double f3, double fb, double clip_level, double input) {
  double k0 = fmin(1., dt * 2 * 3.141592 * f0);
  double k1 = fmin(1., dt * 2 * 3.141592 * f1);
  double k2 = fmin(1., dt * 2 * 3.141592 * f2);
  double k3 = fmin(1., dt * 2 * 3.141592 * f3);
  double old_z0 = ladder->z0;
  ladder->z0 += ladder_shape(input - ladder_shape(ladder->z3 * fb / clip_level) * clip_level - ladder->z0) * k0;
  ladder->z1 += ladder_shape(0.5 * (old_z0 + ladder->z0) - ladder->z1) * k1;
  ladder->z2 += ladder_shape(ladder->z1 - ladder->z2) * k2;
  ladder->z3 += ladder_shape(ladder->z2 - ladder->z3) * k3;
  return ladder->z3;
}

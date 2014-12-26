// (1/(1+s)^3) = 1/(1+3s+3s^2+s3) = 1/(1 + 3jw -3w^2 - jw^3)
// phase = 0 or 180 when imaginary part = 0.
//   jw-jw^3 = 0 -> 3w-w^3 = 0 -> 3-w^2 = 0 -> w^2 = 3 -> w=sqrt(3)
// at this freq, attenuation is
//   1 - 3w^2 = 1-9 = -8.

struct ladder {
  double old_input, old_output;
  double z0, z1, z2; // lp
};

static inline double ladder_shape(double x) {
  return tanh(x);
}

static inline double ladder_tick(struct ladder *ladder, double dt, double f, double fb, double clip_level, double input) {
  double k = fmin(1., dt * 2 * 3.141592 * f);
  double lp = 0.5 * (input + ladder->old_input) - ladder_shape(ladder->z2 * fb / clip_level) * clip_level;
  ladder->old_input = input;
  lp = ladder->z0 += ladder_shape(lp - ladder->z0) * k;
  lp = ladder->z1 += ladder_shape(lp - ladder->z1) * k;
  double old_lp = ladder->z2;
  lp = ladder->z2 += ladder_shape(lp - ladder->z2) * k;
  //lp = ladder->z3 += ladder_shape(lp - ladder->z3) * k;
  ladder->old_output = 0.5 * (old_lp + lp);
  return ladder->old_output;
}

static inline double ladder_shape_gain(double x) {
  return fabs(x) < 1e-20 ? 1 : tanh(x) / x;
}

static inline double ladder_tick_nonlinear_blt(struct ladder *ladder, double dt, double f, double fb, double clip_level, double input) {
  double k = fmin(1., dt * 2 * 3.141592 * f);

  fb = fb * ladder_shape_gain(ladder->z2 * fb / clip_level) * clip_level;

  double k0 = k * ladder_shape_gain(ladder->old_input - ladder->z0 - fb * ladder->z2);
  double k1 = k * ladder_shape_gain(ladder->z0 - ladder->z1);
  double k2 = k * ladder_shape_gain(ladder->z1 - ladder->z2);

  double input_step = input - ladder->old_input;

  double z0_step_constant = k0 * (ladder->old_input - ladder->z0 - fb * ladder->z2);
  double z1_step_constant = k1 * (ladder->z0 - ladder->z1);
  double z2_step_constant = k2 * (ladder->z1 - ladder->z2);

  /*
  double z0_step = z0_step_constant + k0 * 0.5 * (input_step - fb * z2_step);
  double z1_step = z1_step_constant + k1 * 0.5 * z0_step;
  double z2_step = z2_step_constant + k2 * 0.5 * z1_step;
  */
  /*
  double z0_step = z0_step_constant + k0 * 0.5 * (input_step - fb * (z2_step_constant + k2 * 0.5 * (z1_step_constant + k1 * 0.5 * z0_step)));
  double z1_step = z1_step_constant + k1 * 0.5 * z0_step;
  double z2_step = z2_step_constant + k2 * 0.5 * z1_step;
  */
  double z0_step = z0_step_constant + k0 * 0.5 * (input_step - fb * (z2_step_constant + k2 * 0.5 * z1_step_constant));
  z0_step /= 1 + k0 * 0.5 * fb * k2 * 0.5 * k1 * 0.5;
  double z1_step = z1_step_constant + k1 * 0.5 * z0_step;
  double z2_step = z2_step_constant + k2 * 0.5 * z1_step;
  
  ladder->old_input += input_step;
  ladder->z0 += z0_step;
  ladder->z1 += z1_step;
  ladder->z2 += z2_step;

  return ladder->z2;
}

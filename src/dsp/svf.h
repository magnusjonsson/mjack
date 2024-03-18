struct svf {
  double last_input;
  double z1, z2;
};

static inline double svf_tick(struct svf* state, double dt, double freq, double q, double input) {
  freq *= dt;
  if (freq > 0.499) {
    freq = 0.499;
  }
  double omega = 2 * M_PI * freq;
  /*                                                                          
    // compute output                                                          

    double hp = in - q * bp - lp;
    double bp = z1 + f * hp;
    double lp = z2 + f * bp;

    // update state

    z1 += 2 * f * hp;
    z2 += 2 * f * bp;

    // solve for hp

    hp = in - q * (z1 + f * hp) - (z2 + f * (z1 + f * hp))
       = in - q * z1  - q * f * hp - z2 - f * z1 - f * f * hp

    (1 + q * f + f * f) hp = in - (q + f) * z1 - z2;
  */

  double f = tan(0.5 * omega);
  double r = f + q;
  double g = 1 / (f * r + 1);

  // calculate outputs
  double hp = (input - r * state->z1 - state->z2) * g;
  double bp = state->z1 + f * hp;
  double lp = state->z2 + f * bp;

  // update state
  state->z1 = bp + f * hp;
  state->z2 = lp + f * bp;

  return lp;
}

static inline double svf_shape(double x) {
  return fabs(x) < 1e-12 ? 1.0 : tanh(x) / x;
}

static inline double svf_tick_nonlinear(struct svf *state, double dt, double freq, double q, double input) {
  freq *= dt;
  if (freq > 0.499) {
    freq = 0.499;
  }
  double omega = 2 * M_PI * freq;
  /*                                                                          
    // compute output                                                          

    double hp = in - q * bp - lp;
    double bp = z1 + f * tanh(hp) = z1 + k1 * hp where k1 = f * tanh(hp)/hp
    double lp = z2 + f * tanh(lp) = z2 + k2 * lp where k2 = f * tanh(lp)/lp

    // update state

    z1 += 2 * k1 * hp;
    z2 += 2 * k2 * bp;

    // solve for hp

    hp = in - q * (z1 + k1 * hp) - (z2 + k2 * (z1 + k1 * hp))
       = in - q * z1  - q * k1 * hp - z2 - k2 * z1 - k1 * k2 * hp

    (1 + q * k1 + k1 * k2) hp = in - (q + k2) * z1 - z2;
  */

  double f = tan(0.5 * omega);

  double half_delayed_input = (input + state->last_input) * 0.5;
  double fb = half_delayed_input - q * state->z1 - state->z2;
  double k1 = f * svf_shape(fb);
  double k2 = f * svf_shape(state->z2);

  double r = q + k2;
  double g = 1 / (k1 * r + 1);

  // calculate outputs
  double hp = (input - r * state->z1 - state->z2) * g;
  double bp = state->z1 + k1 * hp;
  double lp = state->z2 + k2 * bp;

  // update state
  state->last_input = input;
  state->z1 = bp + k1 * hp;
  state->z2 = lp + k2 * bp;

  return lp;
}

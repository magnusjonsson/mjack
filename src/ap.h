struct ap {
  double state;
};

static inline double ap_tick(struct ap *ap, double dt, double freq, double input) {
  double k = fmax(0, 1 - freq * dt * 2 * M_PI);
  double output = ap->state - input * k;
  ap->state = input + output * k;
  return output;
}

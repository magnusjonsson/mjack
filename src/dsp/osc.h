struct osc {
  double phase;
  double delayed_out;
};

static inline double osc_tick_box(struct osc* osc, double dt, double freq) {
  double phase_inc = freq * dt;
  double curr_out = osc->phase + (1.0/2.0) * phase_inc;
  osc->phase += phase_inc;
  while (osc->phase > 0.5) {
    double t = (osc->phase - 0.5) / phase_inc;
    curr_out -= t;
    osc->phase -= 1.0;
  }
  return curr_out * 2;
}

static inline double osc_tick(struct osc* osc, double dt, double freq) {
  return osc_tick_box(osc, dt, freq);
}


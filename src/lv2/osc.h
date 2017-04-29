struct osc {
  double phase;
};

static void osc_tick_phase(struct osc *self, double freq) {
  double phase = self->phase;
  phase += freq;
  while (phase >= 1.0) {
    phase -= 1.0;
  }
  self->phase = phase;
}

static double osc_tick_sin(struct osc *self, double freq) {
  double phase = self->phase;
  osc_tick_phase(self, freq);
  return sin(2*3.141592*phase);
}

static double osc_tick_saw(struct osc *self, double freq) {
  double phase = self->phase;
  osc_tick_phase(self, freq);
  return 2 * phase - 1;
}

static double osc_tick_duty(struct osc *self, double freq, double duty) {
  double phase = self->phase;
  osc_tick_phase(self, freq);
  return phase < duty ? 1 - duty : - duty;
}

static double osc_tick_saw_box(struct osc *self, double freq) {
  if (freq <= 1e-9) {
    return osc_tick_saw(self, freq);
  }
  double phase0 = self->phase - 0.5;
  osc_tick_phase(self, freq);
  double phase1 = self->phase - 0.5;
  double para0 = phase0 * phase0;
  double para1 = phase1 * phase1;
  return (para1 - para0) / freq;
}

static double osc_tick_triangle_duty(struct osc *self, double freq, double duty) {
  double phase = self->phase;
  osc_tick_phase(self, freq);
  return 8 * fmin(phase * (1 - duty), (1 - phase) * duty) - 1;
}

static double osc_tick_duty_box(struct osc *self, double freq, double duty) {
  if (freq <= 1e-9) {
    return osc_tick_duty(self, freq, duty);
  }
  double phase0 = self->phase;
  osc_tick_phase(self, freq);
  double phase1 = self->phase;

  double integral0 = fmin(phase0 * (1 - duty), (1 - phase0) * duty);
  double integral1 = fmin(phase1 * (1 - duty), (1 - phase1) * duty);
  return (integral1 - integral0) / freq;
}


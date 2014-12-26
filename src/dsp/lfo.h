struct lfo {
  double time; // 0..inf
  double phase; // 0..1
};

static void lfo_trigger(struct lfo *lfo) {
  lfo->time = 0;
}

static double lfo_tick(struct lfo *lfo, double dt, double delay, double freq) {
  lfo->time += dt;
  if (lfo->time < delay) {
    lfo->phase = 0.25;
    return 0.0;
  }
  lfo->phase += dt * freq;
  while(lfo->phase >= 1.0) lfo->phase -= 1.0;
  return (lfo->phase < 0.5 ? lfo->phase : (1 - lfo->phase)) * 4 - 1;
}


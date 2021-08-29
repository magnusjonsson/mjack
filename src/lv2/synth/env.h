struct env {
  double amp;
  double target;
  double gate;
};

static void env_handle_cv(struct env *self, struct cv cv, bool retrigger) {
  if (cv.trigger || (cv.retrigger && retrigger)) {
    self->target = fmax(self->target, cv.gate);
  }
  self->gate = cv.gate;
}

static inline double env_tick(struct env *self, double dt, double speed, double decay, double sustain, double release) {
  self->target += dt * (self->gate > 0 ? decay : release) * (self->gate * sustain - self->target);
  self->amp += dt * speed * (self->target - self->amp);
  return self->amp;
}

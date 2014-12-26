struct simple_env {
  double raw;
  double smoothed;
};

static inline double simple_env_tick(struct simple_env *simple_env, double dt, double speed, double perc) {
  simple_env->raw *= fmax(0.5, 1 - perc * dt);
  simple_env->smoothed += (simple_env->raw - simple_env->smoothed) * fmin(0.5, dt * speed);
  return simple_env->smoothed;
}

static inline void simple_env_trigger(struct simple_env *simple_env) {
  simple_env->raw = 1;
}

static inline void simple_env_untrigger(struct simple_env *simple_env) {
  simple_env->raw = 0;
}

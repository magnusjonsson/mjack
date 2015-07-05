struct adsr_env {
  bool in_attack;
  double value;
};

static inline double adsr_env_tick(struct adsr_env *adsr_env, double dt, double attack, double decay, double sustain, double release, double gate, int shape) {
  if (gate > 0) {
    if (adsr_env->in_attack) {
      adsr_env->value += dt * attack * (1.5 - adsr_env->value);
      if (adsr_env->value >= 1.0) {
	adsr_env->value = 1.0;
	adsr_env->in_attack = false;
      }
    } else {
      double v = adsr_env->value - sustain;
      adsr_env->value = sustain + v * fmax(0.5, fmin(1.0, 1 - dt * decay * pow(v, shape)));
    }
  } else {
    adsr_env->value *= fmax(0.5, 1 - dt * release * pow(adsr_env->value, shape));
  }
  return adsr_env->value;
}

static inline void adsr_env_trigger(struct adsr_env *adsr_env) {
  adsr_env->in_attack = true;
}

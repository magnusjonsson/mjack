struct biquad_coeffs {

  // b2 s^2 + b1 s + b0
  // ------------------
  // a2 s^2 + a1 s + a0


  // b2 z^-2 + b1 z^-1 + b0
  // ------------------
  // a2 z^-2 + a1 z^-1 + a0

  double b2, b1, b0;
  double a2, a1, a0;
};

struct biquad_params {
  double w, Q; // mid frequency and quality factor
  double g2, g1, g0; // square root of high, mid, low gain
};

static inline struct biquad_params biquad_prewarp(struct biquad_params _) {
  double t = tan(0.5 * fmin(3.14, _.w));
  return (struct biquad_params) {
    .w = t, .Q = _.Q / (1.0 + t * t),
    .g2 = _.g2, .g1 = _.g1, .g0 = _.g0,
  };
};

inline struct biquad_coeffs biquad_analog_parametric(struct biquad_params _) {
  return (struct biquad_coeffs) {
    .b2 =       _.g2, .b1 = _.w / _.Q * _.g1, .b0 = _.w * _.w * _.g0,
    .a2 = 1.0 / _.g2, .a1 = _.w / _.Q / _.g1, .a0 = _.w * _.w / _.g0,
  };
}

inline struct biquad_coeffs biquad_analog_parametric_asymmetric(struct biquad_params _) {
  return (struct biquad_coeffs) {
    .b2 = _.g2, .b1 = _.w / _.Q * _.g1, .b0 = _.w * _.w * _.g0,
    .a2 = 1.0,  .a1 = _.w / _.Q       , .a0 = _.w * _.w,
  };
}

static inline struct biquad_coeffs biquad_bilinear_transform(struct biquad_coeffs _) {
  return (struct biquad_coeffs) {
    .b2 = _.b2 - _.b1 + _.b0, .b1 = 2.0 * (_.b0 - _.b2), .b0 = _.b2 + _.b1 + _.b0,
    .a2 = _.a2 - _.a1 + _.a0, .a1 = 2.0 * (_.a0 - _.a2), .a0 = _.a2 + _.a1 + _.a0,
   };
};

static inline struct biquad_coeffs biquad_digital_parametric(struct biquad_params p) {
  return biquad_bilinear_transform(biquad_analog_parametric(biquad_prewarp(p)));
}

static inline struct biquad_coeffs biquad_digital_parametric_asymmetric(struct biquad_params p) {
  return biquad_bilinear_transform(biquad_analog_parametric_asymmetric(biquad_prewarp(p)));
}

static inline struct biquad_coeffs biquad_invert(struct biquad_coeffs _) {
  return (struct biquad_coeffs) {
    .b2 = _.a2, .b1 = _.a1, .b0 = _.a0,
    .a2 = _.b2, .a1 = _.b1, .a0 = _.b0,
  };
}

struct biquad_state {
  double x1, x2;
  double y1, y2;
};

static inline void biquad_process(struct biquad_coeffs c, struct biquad_state *s, const float *in, float *out, int n) {
  FOR(i, n) {
    double x0 = in[i];
    double y0 =
      (1 / c.a0)
      * (+ c.b0 * x0
	 + c.b1 * s->x1
	 + c.b2 * s->x2
	 - c.a1 * s->y1
	 - c.a2 * s->y2);
    out[i] = y0;
    s->x2 = s->x1; s->x1 = x0;
    s->y2 = s->y1; s->y1 = y0;
  }
}
static inline float biquad_tick(struct biquad_coeffs c, struct biquad_state *state, float in) {
  float out;
  biquad_process(c, state, &in, &out, 1);
  return out;
}

struct lpf2 {
  double s0;
  double s1;
};

void lpf2_add_dirac(struct lpf2 *lpf2, double amplitude) {
  lpf2->s0 += amplitude;
}

double lpf2_sample_output(const struct lpf2 *lpf2) {
  return lpf2->s1;
}

// feedback matrix:
//      [ -1       -r ]
// A =  [  1   -1 + r ]
// eigenvalues:
//  (-1-L)(-1-L+r) + r = 0
//  (-1-L+1/2 r)^2 + r - 1/4 r^2 = 0
//  => 
//  L = (1/2 r - 1) +- i sqrt(1 - (1/2 r - 1))
//    = -c +- i s
//  for r > 0, r < 2, so that c > 0, c < 1
//
//  pick L = -c - si and L' = -c + si
//
// eigenvectors:
//  
// [ -1-L     -r ] [ v0]
// [  1   -1-L+r ] [ v1]  = 0
//
// =>  v0 = -r , v1  = 1+L
// and v0' = -r, v1' = 1+L
// and M := 1/|v|^2 = 1/(2*r^2 + s^2)

struct lpf2_transition_function {
  double c;
  double s;
  double v0_re;
  double v1_re;
  double v1_im;
  double m;
  double f;
};

struct lpf2_transition_function make_lpf2_transition_function(double f, double r) {
  double c = 0.5 * r - 1;
  double s = sqrt(1 - c * c); // 1 - c * c = -0.25*r*r + r
  double v0 = -r;
  double v1_re = 1 - c;
  double v1_im = -s;
  double m = 1 / (2 * r * r + s * s); // 1 / (r*r + 2 + 2*c) = 1 / (r*r + r)
}

void lpf2_step(const struct lpf2_transition_function *tf, double f, double dt, double constant) {
  dt *= tf->f;
  
  // state in eigen space
  double S_re = (lpf2->s0 * tf->v0_re + lpf2->s1 * tf->v1_re) * tf->m;
  double S_im = (                       lpf2->s1 * tf->v1_im) * tf->m;

  // first part of constant integral
  S_re += constant * tf->c / dt;
  S_im -= constant * tf->s / dt;
 
  // exp(dt * L)
  double E_re = exp(-dt * tf->c) * cos(-dt * tf->s);
  double E_im = exp(-dt * tf->c) * sin(-dt * tf->s);

  // transform state
  double ES_re = S_re * E_re - S_im * E_im;
  double ES_im = S_re * E_im + S_im * E_re;

  // second part of constant integral
  ES_re -= constant * tf->c / dt;
  ES_im += constant * tf->s / dt;
  
  // back to state space
  lpf->s0 = tf->v0_re * ES_re;
  lpf_>s1 = tf->v1_re * ES_re - v1_im * ES_im;
}


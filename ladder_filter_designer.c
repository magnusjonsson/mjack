#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>

static double complex H_no_feedback(double complex z, int num_poles, double c, int fir_length, double const *fir_coeffs) {
  double complex H = 1.0;
  for (int i = 0; i < num_poles; i++) {
    H *= c / (1 - (1-c) / z);
  }
  double complex fir_response = 0.0;
  double complex zi = 1.0;
  for (int i = 0; i < fir_length; i++) {
    fir_response += fir_coeffs[i] / zi;
    zi *= z;
  }
  H *= fir_response;
  H /= z; // one cycle delay
  return H;
}

int main(int argc, char **argv) {
  int num_poles = 4;
  int num_zeroes = 4;

  double fir_coeffs[4] = { 0.3, 0.4, 0.3, 0 };

  for (int c_percent = 1; c_percent <= 100; c_percent++) {
    double c = c_percent / 100.0;
    for (int f_percent = 0; f_percent <= 50; f_percent++) {
      double f = f_percent / 100.0;
      double w = 2 * 3.141592 * f;
      _Complex double z = cexp(w * 1.0j);
      _Complex double H = H_no_feedback(z, num_poles, c, num_zeroes, fir_coeffs);
      printf("% 1.10f % 1.10f % 1.10f % 1.10f\n", c, f, cabs(H), carg(H) * 180 / 3.141592);
    }
  }
  return 0;
}

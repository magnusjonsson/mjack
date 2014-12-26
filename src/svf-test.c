#include <stdio.h>
#include <stdlib.h>

struct state {
  double x[2];
};

struct state_transition {
  double f[2][2];
};

void compute_state_transition(double dt, double k1, double k2, struct state_transition *out) {
  struct state_transition An={{{1,0},{0,1}}};
  struct state_transition sum;
  //An.f[0][0] = 1; An.f[0][1] = 0;
  //An.f[1][0] = 0; An.f[1][0] = 0;
  sum = An;
  double g = 1;
  for (int i = 0; i < 1; i++) {
    struct state_transition Anp1 =
      {{{An.f[1][0], An.f[1][1]},
	{-k1*An.f[0][0]-k2*An.f[1][0], -k1*An.f[0][1]-k2*An.f[1][1]}}};
    An = Anp1;
    double n = i + 1;
    g *= dt / n;
    An.f[0][0] *= g;
    An.f[0][1] *= g;
    An.f[1][0] *= g;
    An.f[1][1] *= g;
    sum.f[0][0] += An.f[0][0];
    sum.f[0][1] += An.f[0][1];
    sum.f[1][0] += An.f[1][0];
    sum.f[1][1] += An.f[1][1];
  }
  *out = sum;
}

int main(int argc, char **argv) {
  struct state_transition f;
  compute_state_transition(1, atof(argv[1]), atof(argv[2]), &f);
  printf("%f %f\n", f.f[0][0], f.f[0][1]);
  printf("%f %f\n", f.f[1][0], f.f[1][1]);
  return 0;
}

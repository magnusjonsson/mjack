static inline double ap_coeff(double freq) {
    /*

      H(s) = (1 - s/w) / (1 + s/w)

      BLT: s = 2 (1 - z^-1) / (1 + z^-1)
             = 2 (z - 1) / (z + 1)

      H(z) = (1 - 2/w (z-1) / (z+1)) / (1 + 2/w (z-1) / (z+1))
           = ((z+1) - 2/w (z-1)) / ((z+1) + 2/w (z-1))
           = ((1-2/w)z + (1+2/w)) / ((1+2/w)z + (1-2/w))
           = (az + 1) / (z + a)

      a = (1-2/w)/(1+2/w)
        = (w-2)/(w+2)


      w = 2 tan (w_digital / 2)

      (((
      a = (2 tan - 2) / (2 tan + 2)
        = (tan - 1) / (tan + 1)
        = (sin/cos - 1) / (sin + cos)
        = (sin - cos) / (sin + cos) 
      )))

               +--------(k)---+
               V              |
      X ----->(+)-+->[ z^-1 ]-+->(+)--> Y
                  |               A
                  +-----(-k)------+

     U = (X + kUz^-1)
     U (1 - kz^-1) = X
     U/X = 1 / (1 - kz^-1)

     Y = -kU + Uz^-1
     Y/U = (k - z^-1)

     Y/X = (k - z^-1) / (1 - kz^-1)
   */

  if (freq > 0.49) freq = 0.49;
  double w_digital = 2 * 3.141592 * freq;
  double w_analog = 2 * tan(w_digital / 2);
  double apcoeff = -(w_analog-2)/(w_analog+2);
  return apcoeff;
}

static inline double ap_tick(double* state, double in, double apcoeff) {
  double out = *state;
  in += apcoeff * out;
  out -= apcoeff * in;
  *state = in;
  return out;
}

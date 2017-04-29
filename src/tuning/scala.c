#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define FOR(var,limit) for(int var = 0; var < limit; ++var)

static char *read_scala_line(char *buf, size_t len, FILE *fp) {
  while (true) {
    if (!fgets(buf, len, fp)) {
      return NULL;
    }
    if (buf[0] != '!') {
      return buf;
    }
  }
}

bool load_scala_file(const char *filename, float out_cents[static 128], float out_freq[static 128]) {
  FILE *fp = fopen(filename, "rt");
  if (!fp) {
    fprintf(stdout, "Could not open scala file %s\n", filename);
    goto err;
  }
  char buf[1024];
  char *comment = read_scala_line(buf, sizeof buf, fp);
  if (!comment) {
    fprintf(stdout, "Missing comment line in %s\n", filename);
    goto err;
  }
  char *count = read_scala_line(buf, sizeof buf, fp);
  if (!count) {
    fprintf(stdout, "Missing number of scale degrees in %s\n", filename);
    goto err;
  }
  int n = atoi(count);
  if (n <= 0) {
    fprintf(stdout, "Too few scale degrees in %s: %i\n", filename, n);
    goto err;
  }
  if (n >= 128) {
    fprintf(stdout, "Too many scale degrees in %s: %i\n", filename, n); 
    goto err;
  }
  float cents[128];
  FOR(i, n) {
    char *ratio_str = read_scala_line(buf, sizeof buf, fp);
    if (!ratio_str) {
      fprintf(stdout, "Could not read scale degree %i in %s\n", i + 1, filename); 
      goto err;
    }
    char *slash = strchr(ratio_str, '/');
    if (slash) {
      *slash = '\0';
      int numer = atoi(ratio_str);
      int denom = atoi(slash + 1);
      if (numer <= 0 || denom <= 0) {
	fprintf(stdout, "Could not parse ratio in %s\n", filename); 
	goto err;
      }
      cents[i] = log((float) numer / (float) denom) / log(2.0) * 1200.0;
    } else {
      char *end = NULL;
      cents[i] = strtof(ratio_str, &end);
      if (end == ratio_str) {
	fprintf(stdout, "Could not parse cents value in %s\n", filename); 
	goto err;
      }
    }
  }
  FOR(i, 128) {
    int note = (i - 72) % n;
    int oct = (i - 72) / n;
    if (note < 0) {
      note += n;
      oct -= 1;
    }
    float key_cents = 300 + cents[n-1] * oct + (note == 0 ? 1.0 : cents[note-1]);
    float key_freq = 440.0 * pow(2.0, key_cents / 1200.0);
    if (out_freq) out_freq[i] = key_freq;
    if (out_cents) out_cents[i] = key_cents;
  }
  return true;
 err:
  if (fp) {
    fclose(fp);
  }
  return false;
}

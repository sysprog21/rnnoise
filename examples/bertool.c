#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define FRAME_SIZE 480

int main(int argc, char **argv) {
  int i, bt = 0,  bd = 0;
  double sqsum = 0;
  FILE *f1, *f2;
  if (argc!=3) {
    fprintf(stderr, "usage: %s <file a> <file b>\n", argv[0]);
    return 1;
  }
  f1 = fopen(argv[1], "rb");
  f2 = fopen(argv[2], "rb");
  while (!feof(f1)) {
    int16_t a[FRAME_SIZE];
    int16_t b[FRAME_SIZE];
    int br1 = fread(a, sizeof(int16_t), FRAME_SIZE, f1);
    int br2 = fread(b, sizeof(int16_t), FRAME_SIZE, f2);
    if (br1 != br2 || br1 < FRAME_SIZE) break;
    for (i=0;i<FRAME_SIZE;i++) {
      bd += __builtin_popcount(a[i] ^ b[i]);
      sqsum += pow(a[i] - b[i], 2);
    }
    bt += 8 * sizeof(int16_t) * FRAME_SIZE;
  }
  fprintf(stdout, "BER: [%d/%d] (%3.3f%%) RMSD: %3.5f\n", bd, bt, (100.0*bd)/bt, sqrt(sqsum/(bt/16)));
  fclose(f1);
  fclose(f2);
  return 0;
}

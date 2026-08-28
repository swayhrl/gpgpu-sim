#include "l2-char-stats.h"

#include <assert.h>

int main() {
  l2_char_occ_stats stats;
  stats.init(4);
  const unsigned values[] = {0, 0, 1, 2, 2, 2, 3, 4, 4, 4};
  for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
    stats.sample(values[i]);
  assert(stats.samples == 10);
  assert(stats.sum == 22);
  assert(stats.percentile(50, 100) == 2);
  assert(stats.percentile(95, 100) == 4);
  assert(stats.maximum == 4);
  return 0;
}

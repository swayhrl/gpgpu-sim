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

  // ROP uses a compact sparse histogram; percentile semantics must remain
  // identical without allocating every value below a transient high watermark.
  l2_char_occ_stats sparse;
  sparse.init(0, true);
  sparse.sample(0);
  sparse.sample(2);
  sparse.sample(2);
  sparse.sample(1000000);
  assert(sparse.samples == 4);
  assert(sparse.percentile(50, 100) == 2);
  assert(sparse.percentile(95, 100) == 1000000);
  assert(sparse.maximum == 1000000);
  return 0;
}

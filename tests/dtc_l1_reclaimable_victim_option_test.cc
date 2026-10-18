#include "shader.h"

// Production registration/parser, without GPU/kernel/native workload startup.
int main() {
  for (int value : {-1, 0, 1}) {
    shader_core_config cfg(nullptr);
    option_parser_t parser = option_parser_create();
    cfg.reg_options(parser);
    const char *argv[] = {"af-option-test",
        "-gpgpu_dtc_l1_reclaimable_victim_fallback", value == 1 ? "1" : "0"};
    option_parser_cmdline(parser, value < 0 ? 1 : 3, argv);
    assert(cfg.dtc_l1_reclaimable_victim_fallback == (value == 1));
    option_parser_destroy(parser);
  }
}

#pragma once
// One frozen five-input diagnostic vector. No changes to lower DRAM/ICNT.
namespace ep_l2_large {
constexpr unsigned scale=64, sets=4096, ways=16, resident=65536;
constexpr unsigned bypass=8192, wb=1024, wad=8192, lines=8192;
constexpr unsigned descriptors=32768, per_address=2048, missq=8192;
constexpr unsigned resultq=4096, i2l=4096, l2d=8192, d2l=4096, l2i=4096;
}

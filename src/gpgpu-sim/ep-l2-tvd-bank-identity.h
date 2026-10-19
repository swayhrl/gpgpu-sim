#ifndef EP_L2_TVD_BANK_IDENTITY_H
#define EP_L2_TVD_BANK_IDENTITY_H

// Pure comparison on the existing finite bank scoreboard. No allocator,
// address index, data carrier, service port or ready oracle is added here.
// A's adapter gates this extended identity to dirty-victim mode; default OFF
// still uses the exact parent's role/id/write matching.
namespace ep_l2 {
template <class Pending, class Role>
inline bool tvd_bank_request_matches(const Pending &p, Role role, unsigned id,
                                    bool write, unsigned client, unsigned token,
                                    unsigned generation, bool enabled) {
  return p.r == role && p.id == id && p.write == write &&
         (!enabled || (p.client == client && p.token == token &&
                       p.generation == generation));
}
}  // namespace ep_l2
#endif

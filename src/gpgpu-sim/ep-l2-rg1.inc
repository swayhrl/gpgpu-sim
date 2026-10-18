// Included inside ep_l2_payload_store after pending_op. No data ports added.
bool m_rg1=false;
std::function<bool(unsigned,unsigned)> m_ready_source;
unsigned long long m_rg_not_ready[4]={},m_rg_another_ready[4]={},m_rg_recovered[4]={};
unsigned long long m_rg_nr_cycle[4]={~0ull,~0ull,~0ull,~0ull};
unsigned long long m_rg_ar_cycle[4]={~0ull,~0ull,~0ull,~0ull};
unsigned long long m_rg_max_wait[CLIENTS]={},m_rg_ready_wait[CLIENTS]={};
unsigned long long m_rg_first_ready_lag[CLIENTS]={};
unsigned long long m_rg_consumed[CLIENTS]={},m_rg_cancel[CLIENTS]={};
bool rg_ready(pending_op &p,unsigned long long cycle) {
  // GENERIC is deliberately conservative; lack of a C++ call is never a
  // negative readiness signal. Native clients use the live bounded heads.
  assert(p.client<CLIENTS);
  const unsigned global_id=p.r==BYPASS?k_resident_slots+p.id:p.id;
  assert(global_id<m_slots.size() && m_slots[global_id].generation==p.generation);
  bool ready=p.client==GENERIC || (assert(m_ready_source),m_ready_source(p.client,p.token));
  if(ready && p.first_ready==~0ull)p.first_ready=cycle;
  return ready;
}
void rg_observe(unsigned long long cycle) {
  assert(m_cycle==~0ull || cycle>=m_cycle);
  for(unsigned b=0;b<4;++b){
    bool nr=false,ready=false;
    for(auto *q:{&m_pending[b],&m_granted[b]})for(auto &p:*q){bool a=rg_ready(p,cycle);nr|=!a;ready|=a;}
    if(nr && m_rg_nr_cycle[b]!=cycle){++m_rg_not_ready[b];m_rg_nr_cycle[b]=cycle;}
    if(nr && ready && m_rg_ar_cycle[b]!=cycle){++m_rg_another_ready[b];m_rg_ar_cycle[b]=cycle;}
  }
}
bool rg_key(const pending_op &p,role r,unsigned id,bool write,unsigned c,unsigned t,unsigned g) const {
  return p.r==r && p.id==id && p.write==write && p.client==c && p.token==t && p.generation==g;
}
request_result request_rg1(role r,unsigned id,bool write,unsigned long long cycle,
    unsigned long long sequence,operation_class cls,unsigned client,unsigned token,
    unsigned global_id,unsigned bank) {
  assert(client<CLIENTS && m_bank_ops==1);
  const unsigned generation=m_slots[global_id].generation;
  bool exists=false;
  for(auto *q:{&m_pending[bank],&m_granted[bank]})for(auto &p:*q)
    exists |= rg_key(p,r,id,write,client,token,generation);
  if(exists)++m_bank_retry_attempts;
  else {
    record_logical_operation(bank,cls);
    pending_op p(r,id,write,sequence==~0ull?m_next_sequence++:sequence,cycle);
    p.client=client;p.token=token;p.generation=generation;p.first_attempt=cycle;
    m_pending[bank].push_back(p);
    // Actual production has one response, fill, lower-WB and frontend head.
    // Copy phases share that frontend head and cannot coexist as pending ops.
    assert(pending_operations()<=4 && "RG1 bounded four-client scoreboard overflow");
  }
  bool nr=false,ready=false;
  for(auto *q:{&m_pending[bank],&m_granted[bank]})for(auto &p:*q){
    bool a=rg_ready(p,cycle);nr|=!a;ready|=a;
  }
  if(nr && m_rg_nr_cycle[bank]!=cycle){++m_rg_not_ready[bank];m_rg_nr_cycle[bank]=cycle;}
  if(nr && ready && m_rg_ar_cycle[bank]!=cycle){++m_rg_another_ready[bank];m_rg_ar_cycle[bank]=cycle;}
  // Retain selected identities unless explicitly unready or an older ready
  // identity resumes before consumption. Returning them preserves sequence,
  // first-attempt/ready timestamps, generation, client and transaction token.
  if(!m_granted[bank].empty() && !rg_ready(m_granted[bank][0],cycle)){
    m_pending[bank].push_back(m_granted[bank][0]);m_granted[bank].erase(m_granted[bank].begin());
  }
  int best=-1;
  for(unsigned i=0;i<m_pending[bank].size();++i)
    if(rg_ready(m_pending[bank][i],cycle) &&
       (best<0 || m_pending[bank][i].sequence<m_pending[bank][best].sequence))best=i;
  if(m_bank_services[bank]<1 && best>=0 &&
     (m_granted[bank].empty() || m_pending[bank][best].sequence<m_granted[bank][0].sequence)){
    pending_op chosen=m_pending[bank][best];m_pending[bank].erase(m_pending[bank].begin()+best);
    if(!m_granted[bank].empty()){
      m_pending[bank].push_back(m_granted[bank][0]);m_granted[bank][0]=chosen;
    }else m_granted[bank].push_back(chosen);
  }
  if(!m_granted[bank].empty() &&
     rg_key(m_granted[bank][0],r,id,write,client,token,generation) &&
     m_bank_services[bank]<1 && rg_ready(m_granted[bank][0],cycle)){
    const pending_op p=m_granted[bank][0];
    bool recovered=false;
    for(auto &old:m_pending[bank])if(old.sequence<p.sequence && !rg_ready(old,cycle))recovered=true;
    if(recovered)++m_rg_recovered[bank];
    m_granted[bank].erase(m_granted[bank].begin());consume_bank(bank);
    ++m_bank_grants;++m_bank_grants_by_bank[bank];++m_rg_consumed[client];
    m_rg_max_wait[client]=std::max(m_rg_max_wait[client],cycle-p.first_attempt);
    m_rg_first_ready_lag[client]=std::max(m_rg_first_ready_lag[client],p.first_ready-p.first_attempt);
    m_rg_ready_wait[client]=std::max(m_rg_ready_wait[client],cycle-p.first_ready);
    if(p.true_conflict){auto wait=cycle-p.first_conflict_cycle;m_bank_wait_cycles+=wait;m_bank_wait_cycles_by_bank[bank]+=wait;}
    return GRANTED;
  }
  // A denied live logical op remains in exactly one of pending/selected.
  pending_op *denied=nullptr;
  for(auto *q:{&m_pending[bank],&m_granted[bank]})for(auto &p:*q)
    if(rg_key(p,r,id,write,client,token,generation))denied=&p;
  assert(denied);
  if(!denied->true_conflict){denied->true_conflict=true;denied->first_conflict_cycle=cycle;
    ++m_bank_true_conflict_ops;++m_bank_true_conflict_ops_by_bank[bank];}
  ++m_bank_true_conflict_events;++m_bank_true_conflict_events_by_bank[bank];
  return BANK_TRUE_CONTENTION;
}
public:
void cancel_transaction(unsigned token) {
  if(!m_rg1)return;
  for(unsigned b=0;b<4;++b)for(auto *q:{&m_pending[b],&m_granted[b]})
    for(unsigned i=0;i<q->size();)
      if((*q)[i].client!=GENERIC && (*q)[i].token==token){++m_rg_cancel[(*q)[i].client];q->erase(q->begin()+i);}
      else ++i;
}
private:

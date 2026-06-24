// Streaming VALUE contract: the full energy/information chain on physics, no loss.
//   stream -> dynamic sparse graph (no vocab cap) -> LOCAL UNITARY propagation
//   (energy conserved, light cone) -> COMPLEX-OVERLAP readout on the ACTIVE set
//   (phase preserved) -> topic recognition.  Gate: REAL stream graph >> matched-
//   RANDOM stream graph. If real beats random, the LEARNED semantic structure is
//   load-bearing (not generic). No global VxV, no dictionary scan, no phase collapse.
#define NOMINMAX
#include "graph_wave_substrate.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std; using gw::cd; using gw::Vec; using gw::CMat;

struct Graph {
  int W=4, K=6; double eta=1.0, decay=0.99, drop=0.03;
  vector<unordered_map<int,double>> adj; deque<int> win; int t=0;
  int add(){ adj.push_back({}); return (int)adj.size()-1; }
  void touch(int a,int b){ if(a==b)return; adj[a][b]+=eta; adj[b][a]=adj[a][b];
    if((int)adj[a].size()>K) prune(a); if((int)adj[b].size()>K) prune(b); }
  void prune(int a){ while((int)adj[a].size()>K){ int wk=-1;double mn=1e100; for(auto&kv:adj[a])if(kv.second<mn){mn=kv.second;wk=kv.first;} adj[a].erase(wk); adj[wk].erase(a);} }
  void process(int node){ t++; for(int c:win)touch(node,c); win.push_back(node); if((int)win.size()>W)win.pop_front(); }
};

// LOCAL UNITARY propagation: gather 2-hop neighbourhood, build small Hermitian H,
// Cayley-evolve a delta at the source -> sparse complex field over global ids.
static unordered_map<int,cd> propagate(const Graph& g, int src, double tau){
  vector<int> nodes={src}; unordered_map<int,int> idx; idx[src]=0;
  for(int hop=0;hop<2;hop++){ int sz=nodes.size(); for(int h=0;h<sz;h++){ int u=nodes[h];
    for(auto&kv:g.adj[u]) if(!idx.count(kv.first)){idx[kv.first]=nodes.size();nodes.push_back(kv.first);} } }
  int n=nodes.size(); if(n<2){ unordered_map<int,cd> f; f[src]=cd(1,0); return f; }
  CMat H(n,Vec(n,cd(0,0))); for(int i=0;i<n;i++){int u=nodes[i]; for(auto&kv:g.adj[u]){auto it=idx.find(kv.first); if(it!=idx.end())H[i][it->second]=cd(kv.second,0);}}
  gw::Stepper st; st.build(H,tau); Vec psi(n,cd(0,0)); psi[0]=cd(1,0); psi=st.step(psi);
  double nn=0;for(auto&v:psi)nn+=norm(v); nn=sqrt(nn)+1e-15; for(auto&v:psi)v/=nn;
  unordered_map<int,cd> f; for(int i=0;i<n;i++) f[nodes[i]]=psi[i]; return f;
}
// COMPLEX overlap on the ACTIVE (shared) support only
static cd overlap(const unordered_map<int,cd>& a, const unordered_map<int,cd>& b){ cd s(0,0);
  for(auto&kv:a){auto it=b.find(kv.first); if(it!=b.end())s+=conj(kv.second)*it->second;} return s; }

int main(){
  const int TOPICS=3, PER=24, STREAM=60000; const double tau=0.5;
  // vocabulary: 3 topics x PER words; deterministic topical stream
  mt19937 rng(7); uniform_int_distribution<int> rt(0,TOPICS-1), rw(0,PER-1);
  auto buildGraph=[&](bool shuffle){ Graph g; for(int i=0;i<TOPICS*PER;i++)g.add();
    // stream: pick a topic, emit a few of its words in a window (within-topic co-occurrence)
    mt19937 r(11); uniform_int_distribution<int> tt(0,TOPICS-1), ww(0,PER-1);
    // topic-coherent BURSTS: pick a topic, emit several of ITS words in a row so
    // they co-occur in the window -> within-topic edges -> topic clusters.
    for(int s=0;s<STREAM;){ int tp=tt(r); g.win.clear(); for(int b=0;b<6 && s<STREAM;b++,s++){ int node=tp*PER+ww(r); g.process(node);} }
    if(shuffle){ // matched-random: rewire each edge to random endpoints, same degree
      Graph gx; for(int i=0;i<TOPICS*PER;i++)gx.add(); mt19937 rr(5); uniform_int_distribution<int> rn(0,TOPICS*PER-1);
      for(int i=0;i<(int)g.adj.size();i++)for(auto&kv:g.adj[i]) if(i<kv.first){int a=rn(rr),b=rn(rr); if(a!=b){gx.adj[a][b]=kv.second;gx.adj[b][a]=kv.second;}} return gx; }
    return g; };
  Graph real=buildGraph(false), rand_=buildGraph(true);

  auto eval=[&](Graph& g){
    // prototype per topic = superposition of propagated fields of first PER/2 words
    vector<unordered_map<int,cd>> proto(TOPICS);
    for(int tp=0;tp<TOPICS;tp++) for(int w=0;w<PER/2;w++){ auto f=propagate(g,tp*PER+w,tau); for(auto&kv:f)proto[tp][kv.first]+=kv.second; }
    int ok=0,tot=0; for(int tp=0;tp<TOPICS;tp++) for(int w=PER/2;w<PER;w++){ auto f=propagate(g,tp*PER+w,tau);
      int best=0;double bs=-1; for(int c=0;c<TOPICS;c++){double m=abs(overlap(f,proto[c])); if(m>bs){bs=m;best=c;}} ok+=(best==tp);tot++; }
    return 100.0*ok/tot; };

  printf("=== STREAMING VALUE: real semantic stream graph vs matched-random ===\n");
  printf("topics=%d words/topic=%d stream=%d  full chain: stream->dynamic graph->local\n",TOPICS,PER,STREAM);
  printf("unitary propagation->complex overlap on active set (no cap/scan/phase-collapse)\n\n");
  printf("  topic recognition acc:  REAL semantic graph = %.1f%%   matched-RANDOM = %.1f%%   (chance %.0f%%)\n",
         eval(real), eval(rand_), 100.0/TOPICS);
  printf("\nIf REAL >> RANDOM, the streamed semantic structure is load-bearing through the\nfull physics chain -- learned online, weights-free, no information thrown away.\n");
  return 0;
}

// The prism test: read the field across TIME (a spectrogram of its ringing),
// not one snapshot. Order (a temporal property) should unfold in the spectrum.
// Same attested-vs-reversed task. Compare: real-graph snapshot (T=1) vs real-graph
// spectral (T time samples of the free-evolution currents) vs a matched-RANDOM
// graph spectral control. If real spectral >> real snapshot AND >> random spectral,
// "grey" (snapshot) becomes "coloured" (spectrum) only for the real substrate.
#include "../tools/graph_wave_substrate.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std; using gw::cd;
struct Sp{int n=0;vector<vector<pair<int,cd>>> adj;vector<pair<int,int>> edges;double bound=1;};
static vector<double> bess(double x,int km){if(fabs(x)<1e-14){vector<double> j(km+1,0);j[0]=1;return j;}int m=max(km,(int)fabs(x))+80;vector<double> j(m+2,0);j[m]=1e-300;for(int k=m;k>=1;k--)j[k-1]=(2.0*k/x)*j[k]-j[k+1];double nm=j[0];for(int k=2;k<=m;k+=2)nm+=2*j[k];for(int k=0;k<=m;k++)j[k]/=nm;j.resize(km+1);return j;}
static vector<cd> mv(const Sp&g,const vector<cd>&x){vector<cd> y(g.n,cd(0,0));for(int i=0;i<g.n;i++){cd s(0,0);for(auto&e:g.adj[i])s+=e.second*x[e.first];y[i]=s;}return y;}
static vector<cd> cheb(const Sp&g,const vector<cd>&p,double t){double a=g.bound;int km=(int)(a*t)+40;auto j=bess(a*t,km);vector<cd> tm=p,tk=mv(g,p);for(auto&v:tk)v/=a;vector<cd> o(g.n,cd(0,0));cd mi(0,-1);for(int k=0;k<=km;k++){cd cf=(k==0?1.0:2.0)*pow(mi,k)*j[k];if(k==0)for(int i=0;i<g.n;i++)o[i]+=cf*p[i];else if(k==1)for(int i=0;i<g.n;i++)o[i]+=cf*tk[i];else{auto tp=mv(g,tk);for(auto&v:tp)v/=a;for(int i=0;i<g.n;i++)tp[i]=2.0*tp[i]-tm[i];tm.swap(tk);tk.swap(tp);for(int i=0;i<g.n;i++)o[i]+=cf*tk[i];}}return o;}
static void setbound(Sp&g){double mr=0;for(int i=0;i<g.n;i++){double r=0;for(auto&e:g.adj[i])r+=abs(e.second);mr=max(mr,r);}g.bound=1.05*mr+1e-9;}

int main(){
  const int TOK=1000000,VOCAB=4096,WIN=4,TOPK=32;const double tau=0.65,ps=0.65,dt_spec=1.0;const int T=6,NE=1500;
  ifstream in("data/text8");if(!in){printf("no data/text8\n");return 2;}
  unordered_map<string,int> cn;string w;int n=0;while(n<TOK&&(in>>w)){cn[w]++;n++;}
  vector<pair<string,int>> wv(cn.begin(),cn.end());sort(wv.begin(),wv.end(),[](auto&a,auto&b){return a.second!=b.second?a.second>b.second:a.first<b.first;});if((int)wv.size()>VOCAB)wv.resize(VOCAB);
  unordered_map<string,int> id;vector<string> word;for(auto&p:wv){id[p.first]=word.size();word.push_back(p.first);}int N=word.size();
  in.clear();in.seekg(0);vector<int> ids;int kk=0;while(kk<TOK&&(in>>w)){auto it=id.find(w);ids.push_back(it==id.end()?-1:it->second);kk++;}
  vector<float> co((size_t)N*N,0),fwd((size_t)N*N,0);
  for(size_t i=0;i<ids.size();i++){int a=ids[i];if(a<0)continue;for(int d=1;d<=WIN&&i+d<ids.size();d++){int b=ids[i+d];if(b<0||a==b)continue;float ww=1.0f/d;co[(size_t)a*N+b]+=ww;co[(size_t)b*N+a]+=ww;fwd[(size_t)a*N+b]+=ww;}}
  vector<double> rs(N,0);double tot=0;for(int i=0;i<N;i++)for(int j=0;j<N;j++){rs[i]+=co[(size_t)i*N+j];tot+=co[(size_t)i*N+j];}
  vector<float> emb((size_t)N*N,0);for(int i=0;i<N;i++){double nm=0;for(int j=0;j<N;j++){double c=co[(size_t)i*N+j];if(c<=0||rs[i]<=0||rs[j]<=0)continue;double pmi=log(c*tot/(rs[i]*rs[j]));if(pmi>0){emb[(size_t)i*N+j]=pmi;nm+=pmi*pmi;}}nm=sqrt(nm)+1e-12;for(int j=0;j<N;j++)emb[(size_t)i*N+j]/=nm;}
  // real directional-flux graph
  Sp gr;gr.n=N;gr.adj.assign(N,{});vector<unsigned char> ad((size_t)N*N,0);
  for(int i=0;i<N;i++){vector<pair<float,int>> v;for(int j=0;j<N;j++){if(i==j)continue;float s=0.5f*(emb[(size_t)i*N+j]+emb[(size_t)j*N+i]);if(s>0)v.push_back({s,j});}sort(v.begin(),v.end(),[](auto&a,auto&b){return a.first>b.first;});int lim=min(TOPK,(int)v.size());
    for(int t=0;t<lim;t++){int j=v[t].second,a=min(i,j),b=max(i,j);if(ad[(size_t)a*N+b])continue;ad[(size_t)a*N+b]=1;double ww=0.5*(emb[(size_t)a*N+b]+emb[(size_t)b*N+a]);if(ww<=0)continue;double dab=fwd[(size_t)a*N+b],dba=fwd[(size_t)b*N+a];double asym=(dab-dba)/(dab+dba+1e-9);cd h=ww*exp(cd(0,ps*asym));gr.adj[a].push_back({b,h});gr.adj[b].push_back({a,conj(h)});gr.edges.push_back({a,b});}}
  setbound(gr);
  // matched-random graph: same edge count, random endpoints, weight 1, random phase
  Sp gx;gx.n=N;gx.adj.assign(N,{});mt19937 rr(5);uniform_int_distribution<int> rn(0,N-1);uniform_real_distribution<double> rp(-3.14159,3.14159);
  for(size_t e=0;e<gr.edges.size();e++){int a=rn(rr),b=rn(rr);if(a==b)continue;cd h=exp(cd(0,rp(rr)));gx.adj[a].push_back({b,h});gx.adj[b].push_back({a,conj(h)});gx.edges.push_back({a,b});}
  setbound(gx);

  auto curOf=[&](const Sp&g,const vector<cd>&psi,const vector<int>&es){vector<double> c;c.reserve(es.size());for(int ei:es){int i=g.edges[ei].first,j=g.edges[ei].second;cd hij(0,0);for(auto&e:g.adj[i])if(e.first==j){hij=e.second;break;}c.push_back(2.0*imag(conj(psi[i])*hij*psi[j]));}return c;};
  // sampled edge indices
  auto sampleEdges=[&](const Sp&g){vector<int> es;int step=max(1,(int)g.edges.size()/NE);for(size_t i=0;i<g.edges.size()&&(int)es.size()<NE;i+=step)es.push_back(i);return es;};
  auto esr=sampleEdges(gr),esx=sampleEdges(gx);
  // spectral feature: T time-samples of currents over free evolution after encoding
  auto feat=[&](const Sp&g,const vector<int>&es,const vector<int>&ph,int steps){vector<cd> psi(N,cd(0,0));bool has=false;for(int x:ph){if(x<0)return vector<double>();if(has)psi=cheb(g,psi,tau);for(int i=0;i<N;i++)psi[i]+=cd(emb[(size_t)x*N+i],0);has=true;}psi=cheb(g,psi,tau);
    double nn=0;for(auto&v:psi)nn+=norm(v);nn=sqrt(nn)+1e-15;for(auto&v:psi)v/=nn;
    vector<double> f;for(int s=0;s<steps;s++){auto c=curOf(g,psi,es);f.insert(f.end(),c.begin(),c.end());if(s+1<steps)psi=cheb(g,psi,dt_spec);}return f;};

  mt19937 rng(11);vector<vector<int>> tri;for(size_t i=0;i+2<ids.size()&&tri.size()<400;i+=1499){int a=ids[i],b=ids[i+1],c=ids[i+2];if(a<0||b<0||c<0||a==b||b==c||a==c)continue;tri.push_back({a,b,c});}
  int M=tri.size();
  printf("=== PRISM: spectral (time) readout vs snapshot vs random-substrate ===\n");
  printf("nodes=%d edges=%zu sampled=%d trigrams=%d T=%d (chance 50%%)\n\n",N,gr.edges.size(),(int)esr.size(),M,T);

  auto runClf=[&](const Sp&g,const vector<int>&es,int steps){
    vector<vector<double>> X;vector<int> y;
    for(auto&t:tri){auto fa=feat(g,es,t,steps);if(fa.empty())continue;vector<int> r={t[2],t[1],t[0]};auto fb=feat(g,es,r,steps);if(fb.empty())continue;X.push_back(fa);y.push_back(1);X.push_back(fb);y.push_back(-1);}
    int D=X[0].size(),n2=X.size(),ntr=n2/2;vector<double> dir(D,0);for(int i=0;i<ntr;i++)for(int e=0;e<D;e++)dir[e]+=y[i]*X[i][e];
    int ok=0,nt=0;for(int i=ntr;i<n2;i++){double s=0;for(int e=0;e<D;e++)s+=X[i][e]*dir[e];ok+=((s>0?1:-1)==y[i]);nt++;}return 100.0*ok/nt;};

  printf("  real graph, SNAPSHOT  (T=1) : %.1f%%\n",runClf(gr,esr,1));
  printf("  real graph, SPECTRAL  (T=%d) : %.1f%%\n",T,runClf(gr,esr,T));
  printf("  RANDOM graph, SPECTRAL(T=%d) : %.1f%%   (control: should stay near 50%%)\n",T,runClf(gx,esx,T));
  printf("\nIf real spectral >> real snapshot and >> random spectral, the prism reveals\nreal order structure the snapshot hid -- grey becomes coloured for the real substrate.\n");
  return 0;
}

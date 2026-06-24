// Check the OTHER half: does the PHASE/order channel carry USABLE information,
// beyond classical FLOW? Task = distinguish an ATTESTED corpus word order from its
// REVERSE (same words, e.g. "new york" vs "york new"). Bag-of-words is order-blind
// (chance). If the substrate's edge-CURRENT (phase) channel separates attested from
// reversed well above 50%, the phase machinery encodes a real linguistic order
// signal -- not just decodable, but useful.
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
struct Sp{int n=0;vector<vector<pair<int,cd>>> adj;vector<pair<int,int>> edges;};
static vector<double> bessel(double x,int km){if(fabs(x)<1e-14){vector<double> j(km+1,0);j[0]=1;return j;}
  int m=max(km,(int)fabs(x))+80;vector<double> j(m+2,0);j[m]=1e-300;for(int k=m;k>=1;k--)j[k-1]=(2.0*k/x)*j[k]-j[k+1];
  double nm=j[0];for(int k=2;k<=m;k+=2)nm+=2*j[k];for(int k=0;k<=m;k++)j[k]/=nm;j.resize(km+1);return j;}
static vector<cd> mv(const Sp&g,const vector<cd>&x){vector<cd> y(g.n,cd(0,0));for(int i=0;i<g.n;i++){cd s(0,0);for(auto&e:g.adj[i])s+=e.second*x[e.first];y[i]=s;}return y;}
static vector<cd> cheb(const Sp&g,const vector<cd>&p,double t,double a){int km=(int)(a*t)+40;auto j=bessel(a*t,km);
  vector<cd> tm=p,tk=mv(g,p);for(auto&v:tk)v/=a;vector<cd> o(g.n,cd(0,0));cd mi(0,-1);
  for(int k=0;k<=km;k++){cd cf=(k==0?1.0:2.0)*pow(mi,k)*j[k];if(k==0)for(int i=0;i<g.n;i++)o[i]+=cf*p[i];
    else if(k==1)for(int i=0;i<g.n;i++)o[i]+=cf*tk[i];
    else{auto tp=mv(g,tk);for(auto&v:tp)v/=a;for(int i=0;i<g.n;i++)tp[i]=2.0*tp[i]-tm[i];tm.swap(tk);tk.swap(tp);for(int i=0;i<g.n;i++)o[i]+=cf*tk[i];}}return o;}

int main(){
  const int TOK=1000000,VOCAB=4096,WIN=4,TOPK=32;const double tau=0.65,pscale=0.65;
  ifstream in("data/text8");if(!in){printf("no data/text8\n");return 2;}
  unordered_map<string,int> cnt;string w;int n=0;while(n<TOK&&(in>>w)){cnt[w]++;n++;}
  vector<pair<string,int>> wv(cnt.begin(),cnt.end());sort(wv.begin(),wv.end(),[](auto&a,auto&b){return a.second!=b.second?a.second>b.second:a.first<b.first;});
  if((int)wv.size()>VOCAB)wv.resize(VOCAB);unordered_map<string,int> id;vector<string> word;
  for(auto&p:wv){id[p.first]=word.size();word.push_back(p.first);}int N=word.size();
  in.clear();in.seekg(0);vector<int> ids;int kk=0;while(kk<TOK&&(in>>w)){auto it=id.find(w);ids.push_back(it==id.end()?-1:it->second);kk++;}
  vector<float> co((size_t)N*N,0),fwd((size_t)N*N,0);
  for(size_t i=0;i<ids.size();i++){int a=ids[i];if(a<0)continue;for(int d=1;d<=WIN&&i+d<ids.size();d++){int b=ids[i+d];if(b<0||a==b)continue;float ww=1.0f/d;
    co[(size_t)a*N+b]+=ww;co[(size_t)b*N+a]+=ww;fwd[(size_t)a*N+b]+=ww;}}
  vector<double> rsum(N,0);double tot=0;for(int i=0;i<N;i++)for(int j=0;j<N;j++){rsum[i]+=co[(size_t)i*N+j];tot+=co[(size_t)i*N+j];}
  vector<float> emb((size_t)N*N,0);for(int i=0;i<N;i++){double nm=0;for(int j=0;j<N;j++){double c=co[(size_t)i*N+j];if(c<=0||rsum[i]<=0||rsum[j]<=0)continue;double pmi=log(c*tot/(rsum[i]*rsum[j]));if(pmi>0){emb[(size_t)i*N+j]=pmi;nm+=pmi*pmi;}}nm=sqrt(nm)+1e-12;for(int j=0;j<N;j++)emb[(size_t)i*N+j]/=nm;}
  // directional-flux graph (Codex style)
  Sp g;g.n=N;g.adj.assign(N,{});vector<unsigned char> ad((size_t)N*N,0);
  for(int i=0;i<N;i++){vector<pair<float,int>> v;for(int j=0;j<N;j++){if(i==j)continue;float s=0.5f*(emb[(size_t)i*N+j]+emb[(size_t)j*N+i]);if(s>0)v.push_back({s,j});}
    sort(v.begin(),v.end(),[](auto&a,auto&b){return a.first>b.first;});int lim=min(TOPK,(int)v.size());
    for(int t=0;t<lim;t++){int j=v[t].second,a=min(i,j),b=max(i,j);if(ad[(size_t)a*N+b])continue;ad[(size_t)a*N+b]=1;
      double ww=0.5*(emb[(size_t)a*N+b]+emb[(size_t)b*N+a]);if(ww<=0)continue;double dab=fwd[(size_t)a*N+b],dba=fwd[(size_t)b*N+a];
      double asym=(dab-dba)/(dab+dba+1e-9);cd h=ww*exp(cd(0,pscale*asym));g.adj[a].push_back({b,h});g.adj[b].push_back({a,conj(h)});g.edges.push_back({a,b});}}
  double mr=0;for(int i=0;i<N;i++){double r=0;for(auto&e:g.adj[i])r+=abs(e.second);mr=max(mr,r);}double bound=1.05*mr+1e-9;
  auto driveCur=[&](const vector<int>&ph){vector<cd> psi(N,cd(0,0));bool has=false;for(int x:ph){if(x<0)return vector<double>();if(has)psi=cheb(g,psi,tau,bound);for(int i=0;i<N;i++)psi[i]+=cd(emb[(size_t)x*N+i],0);has=true;}psi=cheb(g,psi,tau,bound);
    double nn=0;for(auto&v:psi)nn+=norm(v);nn=sqrt(nn)+1e-15;for(auto&v:psi)v/=nn;
    vector<double> cur;cur.reserve(g.edges.size());for(auto&p:g.edges){int i=p.first,j=p.second;cd hij(0,0);for(auto&e:g.adj[i])if(e.first==j){hij=e.second;break;}cur.push_back(2.0*imag(conj(psi[i])*hij*psi[j]));}return cur;};

  // sample attested trigrams (3 consecutive in-vocab distinct words)
  mt19937 rng(11);vector<vector<int>> tri;
  for(size_t i=0;i+2<ids.size()&&tri.size()<600;i+=997){int a=ids[i],b=ids[i+1],c=ids[i+2];if(a<0||b<0||c<0||a==b||b==c||a==c)continue;tri.push_back({a,b,c});}
  int M=tri.size();
  printf("=== ORDER VALUE: phase channel distinguishes attested vs reversed order ===\n");
  printf("tokens=%d nodes=%d edges=%zu trigrams=%d (attested vs reversed; bag=chance 50%%)\n\n",TOK,N,g.edges.size(),M);
  // build current fingerprints for attested(+1) and reversed(-1); split train/test
  vector<vector<double>> X;vector<int> y;
  for(auto&t:tri){auto ca=driveCur(t);if(ca.empty())continue;vector<int> r={t[2],t[1],t[0]};auto cr=driveCur(r);if(cr.empty())continue;
    X.push_back(ca);y.push_back(1);X.push_back(cr);y.push_back(-1);}
  int E=g.edges.size(),n2=X.size();int ntr=n2/2;
  // weights-free prototype direction = mean(attested)-mean(reversed) over train
  vector<double> dir(E,0);int na=0,nb=0;
  for(int i=0;i<ntr;i++){for(int e=0;e<E;e++)dir[e]+=y[i]*X[i][e];}
  // classify test: sign of <X, dir>
  int ok=0,ntst=0;for(int i=ntr;i<n2;i++){double s=0;for(int e=0;e<E;e++)s+=X[i][e]*dir[e];int pred=s>0?1:-1;ok+=(pred==y[i]);ntst++;}
  printf("  PHASE (edge-current) channel : attested-vs-reversed acc = %.1f%%  (chance 50%%)\n",100.0*ok/ntst);
  printf("  bag-of-words                 : 50.0%%  (order-blind by construction)\n");
  printf("\nIf the current channel is well above 50%%, the phase machinery carries a real,\nUSABLE order signal -- value beyond the classical FLOW/aggregation channel.\n");
  return 0;
}

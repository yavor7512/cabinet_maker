#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace cut {
constexpr double eps=1e-6;
struct Part {std::string name; double w,h,t; bool machined=false; double removedVolume=0;};
struct Stock {std::string name; double w,h;};
struct Node;
using Tree=std::shared_ptr<Node>;
struct Node {double w,h; int part=-1; bool rotated=false; char split=0; Tree a,b;};
Tree leaf(const Part&p,int id,bool rotate=false) {return std::make_shared<Node>(Node{rotate?p.h:p.w,rotate?p.w:p.h,id,rotate,0,{},{}});}
Tree join(Tree a,Tree b,char split,double kerf) {
  return std::make_shared<Node>(Node{split=='V'?a->w+kerf+b->w:std::max(a->w,b->w),split=='H'?a->h+kerf+b->h:std::max(a->h,b->h),-1,false,split,a,b});
}
struct Sheet {Stock stock; Tree tree;};
struct Result {std::vector<Sheet> sheets; bool exact=false; std::string reason; std::uint64_t candidates=0;};
struct Settings {double kerf=5,seconds=10; bool rotate=false; int exactLimit=12; std::uint64_t maxCandidates=5000000;};
bool fits(Tree n,const Stock&s){return n->w<=s.w+eps && n->h<=s.h+eps;}
double area(const std::vector<Sheet>&s){double a=0;for(auto&x:s)a+=x.stock.w*x.stock.h;return a;}
// Shelf fallback always produces a valid guillotine tree; it is not called optimal.
Result greedy(const std::vector<Part>&parts,const std::vector<Stock>&stocks,const Settings&cfg) {
  std::vector<int> remaining;for(int i=0;i<(int)parts.size();++i)remaining.push_back(i);
  std::stable_sort(remaining.begin(),remaining.end(),[&](int a,int b){return parts[a].w*parts[a].h>parts[b].w*parts[b].h;});
  Result result;
  while(!remaining.empty()) {
    double best=-1; Sheet chosen;std::vector<int> taken;
    for(const auto&s:stocks) {
      Tree rows,row;std::vector<int> ids;double used=0;
      for(int id:remaining) {
        for(int r=0;r<=(cfg.rotate?1:0);++r) {
          auto p=leaf(parts[id],id,r);Tree trial=row?join(row,p,'V',cfg.kerf):p;
          Tree all=rows?join(rows,trial,'H',cfg.kerf):trial;
          if(fits(all,s)) {row=trial;ids.push_back(id);used+=parts[id].w*parts[id].h;break;}
          all=rows?(row?join(join(rows,row,'H',cfg.kerf),p,'H',cfg.kerf):join(rows,p,'H',cfg.kerf)):(row?join(row,p,'H',cfg.kerf):p);
          if(fits(all,s)) {rows=rows?(row?join(rows,row,'H',cfg.kerf):rows):row;row=p;ids.push_back(id);used+=parts[id].w*parts[id].h;break;}
        }
      }
      if(ids.empty())continue;
      double score=used/(s.w*s.h);
      if(score>best+eps || (std::abs(score-best)<eps && s.w*s.h<chosen.stock.w*chosen.stock.h)) {best=score;chosen={s,rows?join(rows,row,'H',cfg.kerf):row};taken=ids;}
    }
    if(best<0)throw std::runtime_error("A part cannot fit any selected sheet with the selected grain/rotation policy");
    result.sheets.push_back(chosen);
    for(int id:taken)remaining.erase(std::remove(remaining.begin(),remaining.end(),id),remaining.end());
  }
  return result;
}
// Exact subset DP enumerates binary guillotine slicing trees and retains
// nondominated bounding rectangles. Objective: sheet area, then sheet count.
Result optimize(const std::vector<Part>&parts,const std::vector<Stock>&stocks,const Settings&cfg) {
  Result fallback=greedy(parts,stocks,cfg);
  if(parts.size()>(std::size_t)cfg.exactLimit || parts.size()>16) {fallback.reason="part count exceeds exact-search limit";return fallback;}
  const auto start=std::chrono::steady_clock::now();std::uint64_t candidates=0;
  const unsigned count=1u<<parts.size();std::vector<std::vector<Tree>> dp(count);
  auto add=[&](unsigned mask,Tree n) {
    if(++candidates>cfg.maxCandidates || (candidates%1024==0 && std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()>cfg.seconds))throw std::runtime_error("search budget reached");
    if(std::none_of(stocks.begin(),stocks.end(),[&](const Stock&s){return fits(n,s);}))return;
    auto&v=dp[mask];
    for(auto&q:v)if(q->w<=n->w+eps && q->h<=n->h+eps)return;
    v.erase(std::remove_if(v.begin(),v.end(),[&](Tree q){return n->w<=q->w+eps&&n->h<=q->h+eps;}),v.end());v.push_back(n);
  };
  try {
    for(unsigned mask=1;mask<count;++mask) {
      if((mask&(mask-1))==0) {int i=0;while((1u<<i)!=mask)++i;add(mask,leaf(parts[i],i));if(cfg.rotate)add(mask,leaf(parts[i],i,true));continue;}
      for(unsigned a=(mask-1)&mask;a;a=(a-1)&mask) {unsigned b=mask^a;if(!b || a>b)continue;
        for(auto&x:dp[a])for(auto&y:dp[b]) {add(mask,join(x,y,'V',cfg.kerf));add(mask,join(x,y,'H',cfg.kerf));}
      }
    }
    struct Choice {double area=INFINITY;int sheets=0;unsigned subset=0;Sheet sheet;};
    std::vector<Choice> best(count);best[0].area=0;
    for(unsigned mask=1;mask<count;++mask) {
      const unsigned first=mask&(~mask+1);
      for(unsigned sub=mask;sub;sub=(sub-1)&mask) {if(!(sub&first))continue;
        if(++candidates>cfg.maxCandidates || (candidates%1024==0 && std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()>cfg.seconds))throw std::runtime_error("search budget reached");
        const auto&prev=best[mask^sub];if(!std::isfinite(prev.area))continue;
        for(const auto&s:stocks) {
          auto node=std::find_if(dp[sub].begin(),dp[sub].end(),[&](Tree n){return fits(n,s);});if(node==dp[sub].end())continue;
          double a=prev.area+s.w*s.h;int n=prev.sheets+1;
          if(a<best[mask].area-eps || (std::abs(a-best[mask].area)<eps&&n<best[mask].sheets))best[mask]={a,n,sub,{s,*node}};
        }
      }
    }
    Result result;result.exact=true;result.candidates=candidates;
    for(unsigned mask=count-1;mask;mask^=best[mask].subset) {if(!best[mask].subset)throw std::runtime_error("No feasible exact layout");result.sheets.push_back(best[mask].sheet);}
    result.reason="proven minimum sheet area, then sheet count, within rectangular guillotine layouts";return result;
  } catch(const std::runtime_error&e) {fallback.reason=e.what();fallback.candidates=candidates;return fallback;}
}
}

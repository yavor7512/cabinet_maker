#pragma once
#include "nesting.hpp"
#include <random>
#include <numeric>
#include <set>
namespace cut {
// Shared-width columns / shared-height rows can be sized once before their
// crosscuts. Avoid trimming their combined length, which depends on kerf.
bool sharedWidth(Tree n){return n->part<0&&n->split=='H'&&std::abs(n->a->w-n->b->w)<eps;}
bool sharedHeight(Tree n){return n->part<0&&n->split=='V'&&std::abs(n->a->h-n->b->h)<eps;}
// A remainder dimension depends on the actual kerf. Always finish it from
// the kept edge, even when its nominal size already matches the requested size.
int plannedCuts(Tree n,double w,double h,double kerf,int mode,bool fixedW=true,bool fixedH=true) {
  int count=0;
  if((mode==0||n->part>=0||(mode==2&&n->split=='V')||(mode==3&&sharedWidth(n)))&&(w>n->w+eps||!fixedW)) {++count;w=n->w;fixedW=true;}
  if((mode==0||n->part>=0||(mode==2&&n->split=='H')||(mode==3&&sharedHeight(n)))&&(h>n->h+eps||!fixedH)) {++count;h=n->h;fixedH=true;}
  if(n->part>=0)return count;
  if(n->split=='V')return count+1
    +plannedCuts(n->a,n->a->w,h,kerf,mode,true,fixedH)
    +plannedCuts(n->b,w-n->a->w-kerf,h,kerf,mode,false,fixedH);
  return count+1
    +plannedCuts(n->a,w,n->a->h,kerf,mode,fixedW,true)
    +plannedCuts(n->b,w,h-n->a->h-kerf,kerf,mode,fixedW,false);
}
double nodeKerf(Tree n){if(n->part>=0)return 0;return n->split=='V'?n->w-n->a->w-n->b->w:n->h-n->a->h-n->b->h;}
int planMode(Tree n,double w,double h,double kerf){
  int mode=3,best=plannedCuts(n,w,h,kerf,mode);
  for(int candidate:{2,0,1}){int count=plannedCuts(n,w,h,kerf,candidate);if(count<best){mode=candidate;best=count;}}
  return mode;
}
int shopCuts(Tree n,double w,double h){double kerf=nodeKerf(n);return plannedCuts(n,w,h,kerf,planMode(n,w,h,kerf));}
int shopCuts(const Result&r){int n=0;for(auto&s:r.sheets)n+=shopCuts(s.tree,s.stock.w,s.stock.h);return n;}
int turns(Tree n,char previous=0){if(n->part>=0)return 0;return (previous&&previous!=n->split)+turns(n->a,n->split)+turns(n->b,n->split);}
int turns(const Result&r){int n=0;for(auto&s:r.sheets)n+=turns(s.tree);return n;}
Tree chain(const std::vector<Tree>&items,char axis,double kerf){Tree t=items.back();for(int i=(int)items.size()-2;i>=0;--i)t=join(items[i],t,axis,kerf);return t;}
// Multiple strips per sheet; best-fit insertion favors matching strip breadths.
Result strips(const std::vector<Part>&parts,const std::vector<Stock>&stocks,const Settings&cfg,const std::vector<int>&order,char axis) {
  std::vector<int>remaining=order;Result result;
  while(!remaining.empty()) {
    Sheet selected;std::vector<int>selectedIds;double bestEfficiency=-1;int bestCuts=0;
    for(auto&stock:stocks) {
      std::vector<std::vector<Tree>>rows;std::vector<int>taken;double used=0;
      for(int id:remaining) {
        int bestRow=-1;Tree bestLeaf;double bestPenalty=INFINITY;
        for(int rotate=0;rotate<=(cfg.rotate?1:0);++rotate)for(int row=0;row<=(int)rows.size();++row) {
          auto p=leaf(parts[id],id,rotate);auto trial=rows;
          if(row==(int)trial.size())trial.push_back({p});else trial[row].push_back(p);
          std::vector<Tree>bands;for(auto&v:trial)bands.push_back(chain(v,axis=='H'?'V':'H',cfg.kerf));
          auto all=chain(bands,axis,cfg.kerf);if(!fits(all,stock))continue;
          const double breadth=axis=='H'?p->h:p->w;
          double existing=row<(int)rows.size()?(axis=='H'?chain(rows[row],'V',cfg.kerf)->h:chain(rows[row],'H',cfg.kerf)->w):0;
          double penalty=row==(int)rows.size()?1e8+breadth:std::abs(existing-breadth)*1000+std::max(0.0,breadth-existing)*1000;
          if(penalty<bestPenalty){bestPenalty=penalty;bestRow=row;bestLeaf=p;}
        }
        if(bestRow>=0){if(bestRow==(int)rows.size())rows.push_back({bestLeaf});else rows[bestRow].push_back(bestLeaf);taken.push_back(id);used+=parts[id].w*parts[id].h;}
      }
      if(taken.empty())continue;
      // Batch equal-width bands; longest strips first consolidates the waste.
      std::vector<Tree>bands;for(auto&v:rows) {
        std::stable_sort(v.begin(),v.end(),[&](Tree a,Tree b){return axis=='H'?a->w>b->w:a->h>b->h;});
        bands.push_back(chain(v,axis=='H'?'V':'H',cfg.kerf));
      }
      std::stable_sort(bands.begin(),bands.end(),[&](Tree a,Tree b){return axis=='H'?a->h>b->h:a->w>b->w;});
      auto tree=chain(bands,axis,cfg.kerf);double efficiency=used/(stock.w*stock.h);int cuts=shopCuts(tree,stock.w,stock.h);
      if(efficiency>bestEfficiency+eps||(std::abs(efficiency-bestEfficiency)<eps&&cuts<bestCuts)){bestEfficiency=efficiency;bestCuts=cuts;selected={stock,tree};selectedIds=taken;}
    }
    if(selectedIds.empty())throw std::runtime_error("Part cannot fit selected sheets");
    result.sheets.push_back(selected);
    for(int id:selectedIds)remaining.erase(std::remove(remaining.begin(),remaining.end(),id),remaining.end());
  }
  return result;
}
Result optimizeShop(const std::vector<Part>&parts,const std::vector<Stock>&stocks,const Settings&cfg,double extraPercent,int attempts) {
  auto baseline=optimize(parts,stocks,cfg);auto best=baseline;const double budget=area(baseline.sheets)*(1+extraPercent/100);
  std::vector<int>order(parts.size());std::iota(order.begin(),order.end(),0);std::mt19937 rng(20261016);
  for(int trial=0;trial<attempts;++trial){
    if(trial==0)std::stable_sort(order.begin(),order.end(),[&](int a,int b){return parts[a].h>parts[b].h;});
    else if(trial==1)std::stable_sort(order.begin(),order.end(),[&](int a,int b){return parts[a].w>parts[b].w;});
    else if(trial==2)std::stable_sort(order.begin(),order.end(),[&](int a,int b){return parts[a].w*parts[a].h>parts[b].w*parts[b].h;});
    else std::shuffle(order.begin(),order.end(),rng);
    for(char axis:{'H','V'}) {
      auto candidate=strips(parts,stocks,cfg,order,axis);if(area(candidate.sheets)>budget+eps)continue;
      int c=shopCuts(candidate),b=shopCuts(best);
      if(c<b||(c==b&&(turns(candidate)<turns(best)||(turns(candidate)==turns(best)&&area(candidate.sheets)<area(best.sheets)-eps))))best=candidate;
    }
  }
  best.candidates=baseline.candidates;best.exact=false;best.reason="Shop heuristic: minimize actual saw cuts, then slicing-direction changes, then sheet area; area budget relative to material-search result. No global cut-count optimality claim. Baseline "+std::to_string(shopCuts(baseline))+" cuts; selected "+std::to_string(shopCuts(best))+" cuts.";
  return best;
}
}

#pragma once
#include "shop.hpp"
#include <set>

using namespace cut;
struct Placement {int id,sheet;double x,y,w,h;bool rotated;std::string source={};};
struct SawCut {int sheet,stage,sequence=0;char axis;double x,y,w,h,at,length;std::string source={};};

// Geometry stays in sheet coordinates. For H cuts the operator turns the
// subpanel clockwise so its diagram-bottom edge becomes the physical left.
void shopLayout(Tree n,int sheet,double x,double y,double w,double h,double kerf,
                std::vector<Placement>&places,std::vector<SawCut>&cuts,
                char previous=0,int stage=0,int mode=0,bool fixedW=true,bool fixedH=true) {
  auto saw=[&](char axis,double at){
    if(axis!=previous){++stage;previous=axis;}
    cuts.push_back({sheet,stage,0,axis,x,y,w,h,at,axis=='V'?h:w});
  };
  if(n->w>w+eps||n->h>h+eps)throw std::runtime_error("Shop subtree exceeds subpanel");
  if((mode==0||n->part>=0||(mode==2&&n->split=='V')||(mode==3&&sharedWidth(n)))&&(w>n->w+eps||!fixedW)) {
    saw('V',x+n->w);w=n->w;fixedW=true;
  }
  if((mode==0||n->part>=0||(mode==2&&n->split=='H')||(mode==3&&sharedHeight(n)))&&(h>n->h+eps||!fixedH)) {
    saw('H',y+n->h);h=n->h;fixedH=true;
  }
  if(n->part>=0){places.push_back({n->part,sheet,x,y,n->w,n->h,n->rotated});return;}
  if(n->split=='V') {
    saw('V',x+n->a->w);
    shopLayout(n->a,sheet,x,y,n->a->w,h,kerf,places,cuts,previous,stage,mode,true,fixedH);
    shopLayout(n->b,sheet,x+n->a->w+kerf,y,w-n->a->w-kerf,h,kerf,places,cuts,previous,stage,mode,false,fixedH);
  }else{
    saw('H',y+n->a->h);
    shopLayout(n->a,sheet,x,y,w,n->a->h,kerf,places,cuts,previous,stage,mode,fixedW,true);
    shopLayout(n->b,sheet,x,y+n->a->h+kerf,w,h-n->a->h-kerf,kerf,places,cuts,previous,stage,mode,fixedW,false);
  }
}

struct SawTrace {std::vector<std::string> inputs,finished;};
// Track nominal diagram coordinates separately from real subpanel dimensions.
// Every cut is measured afresh from its current kept edge; a smaller blade
// changes the right remainder, never the requested KEEP LEFT dimension.
SawTrace traceSawSequence(const std::vector<SawCut>&cuts,const std::vector<Sheet>&sheets,
                         double plannedKerf,const std::vector<Placement>*places,
                         double actualKerf) {
  if(actualKerf<0||actualKerf>plannedKerf+eps)throw std::runtime_error("Actual kerf exceeds planning allowance");
  struct Panel{int sheet;double x,y,w,h,actualW,actualH;std::string name;};
  std::vector<Panel>panels;SawTrace trace;
  for(int i=0;i<(int)sheets.size();++i)panels.push_back({i+1,0,0,sheets[i].stock.w,sheets[i].stock.h,
    sheets[i].stock.w,sheets[i].stock.h,"S"+std::to_string(i+1)});
  for(std::size_t i=0;i<cuts.size();++i){const auto&c=cuts[i];
    auto p=std::find_if(panels.begin(),panels.end(),[&](const Panel&p){return p.sheet==c.sheet
      &&std::abs(p.x-c.x)<eps&&std::abs(p.y-c.y)<eps&&std::abs(p.w-c.w)<eps&&std::abs(p.h-c.h)<eps;});
    if(p==panels.end())throw std::runtime_error("Cut references a subpanel that does not yet exist");
    if(c.axis!='V'&&c.axis!='H')throw std::runtime_error("Invalid cut axis");
    auto original=*p;panels.erase(p);trace.inputs.push_back(original.name);
    const double d=c.at-(c.axis=='V'?original.x:original.y);
    const double available=c.axis=='V'?original.w:original.h;
    const double actualAvailable=c.axis=='V'?original.actualW:original.actualH;
    if(d<=0||d>available+eps||d>actualAvailable+eps)throw std::runtime_error("Invalid kept-edge cut dimension");
    const auto name="C"+std::to_string(i+1);
    auto keep=original;keep.name=name+"L";
    if(c.axis=='V'){keep.w=d;keep.actualW=d;}else{keep.h=d;keep.actualH=d;}
    panels.push_back(keep);
    const double rest=available-d-plannedKerf;
    if(rest>eps){auto other=original;other.name=name+"R";
      if(c.axis=='V'){other.x+=d+plannedKerf;other.w=rest;other.actualW=actualAvailable-d-actualKerf;}
      else{other.y+=d+plannedKerf;other.h=rest;other.actualH=actualAvailable-d-actualKerf;}
      panels.push_back(other);
    }
  }
  if(places)for(const auto&p:*places){
    auto found=std::find_if(panels.begin(),panels.end(),[&](const Panel&q){return p.sheet==q.sheet
      &&std::abs(p.x-q.x)<eps&&std::abs(p.y-q.y)<eps&&std::abs(p.w-q.w)<eps&&std::abs(p.h-q.h)<eps;});
    if(found==panels.end())throw std::runtime_error("Saw sequence did not produce a required finished blank");
    if(std::abs(found->actualW-p.w)>eps||std::abs(found->actualH-p.h)>eps)
      throw std::runtime_error("Finished blank depends on actual kerf; add a kept-edge finishing cut");
    trace.finished.push_back(found->name);panels.erase(found);
  }
  return trace;
}
void validateSawSequence(const std::vector<SawCut>&cuts,const std::vector<Sheet>&sheets,
                         double kerf,const std::vector<Placement>*places=nullptr) {
  traceSawSequence(cuts,sheets,kerf,places,kerf);
}
void labelSawSequence(std::vector<SawCut>&cuts,const std::vector<Sheet>&sheets,double kerf,
                      std::vector<Placement>&places) {
  const auto trace=traceSawSequence(cuts,sheets,kerf,&places,kerf);
  // Endpoint checks bound the extra stock left by every smaller kerf. Also
  // check an intermediate value independently of the nominal geometry.
  traceSawSequence(cuts,sheets,kerf,&places,0);
  traceSawSequence(cuts,sheets,kerf,&places,kerf/2);
  for(std::size_t i=0;i<cuts.size();++i){cuts[i].sequence=int(i+1);cuts[i].source=trace.inputs[i];}
  for(std::size_t i=0;i<places.size();++i)places[i].source=trace.finished[i];
}

void validate(const std::vector<Placement>&v,const std::vector<Part>&parts,const std::vector<Sheet>&sheets,double kerf) {
  std::set<int>seen;
  for(auto&p:v) {
    if(!seen.insert(p.id).second)throw std::runtime_error("Duplicate cut part");
    auto&s=sheets.at(p.sheet-1).stock;auto&part=parts.at(p.id);
    if(p.x<-eps||p.y<-eps||p.x+p.w>s.w+eps||p.y+p.h>s.h+eps)throw std::runtime_error("Part outside sheet");
    if(std::abs(p.w-(p.rotated?part.h:part.w))>eps||std::abs(p.h-(p.rotated?part.w:part.h))>eps)throw std::runtime_error("Part dimensions changed");
    for(auto&q:v)if(q.sheet==p.sheet&&q.id!=p.id) {
      if(!(p.x+p.w+kerf<=q.x+eps||q.x+q.w+kerf<=p.x+eps||p.y+p.h+kerf<=q.y+eps||q.y+q.h+kerf<=p.y+eps))throw std::runtime_error("Parts overlap or lack kerf clearance");
    }
  }
  if(seen.size()!=parts.size())throw std::runtime_error("Missing cut parts");
}

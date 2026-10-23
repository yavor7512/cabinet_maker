#pragma once
#include "pdf-font.hpp"
// Vector-only shop packet. Dimensions, cut order and placement data remain authoritative.
namespace packet {
inline std::string num(double v){std::ostringstream o;o<<std::fixed<<std::setprecision(2)<<v;auto s=o.str();while(s.size()>1&&s.back()=='0')s.pop_back();if(s.back()=='.')s.pop_back();return s;}
inline std::string escape(const std::string&s){std::string r;for(unsigned char c:s){if(c=='('||c==')'||c=='\\')r+='\\';r+=(c>=32&&c<127)?char(c):'?';}return r;}
inline std::string pid(int i){return "P"+(i<10?std::string("0"):std::string())+std::to_string(i);}
inline const FontData& metrics(){static const auto f=helveticaNeue();return f;}
inline double width(const std::string&s,double size){static const auto advances=[](){std::vector<double>v;std::istringstream in(metrics().widths);double n;while(in>>n)v.push_back(n);return v;}();double w=0;for(unsigned char c:s)w+=advances[(c>=32&&c<=126)?c-32:'?'-32];return w*size/1000;}
inline std::string fit(std::string s,double w,double size){if(width(s,size)<=w)return s;while(!s.empty()&&width(s+"...",size)>w)s.pop_back();return s+"...";}
struct Page {
 std::ostringstream c;
 void text(double x,double y,const std::string&s,double size=10,const std::string&color="0.12 0.16 0.18",bool strong=false){c<<color<<" rg BT /F1 "<<size<<" Tf ";if(strong)c<<"0.25 w 2 Tr "<<color<<" RG ";c<<x<<' '<<y<<" Td ("<<escape(s)<<") Tj 0 Tr ET\n";}
 void right(double x,double y,const std::string&s,double size=10){text(x-width(s,size),y,s,size);}
 void line(double x,double y,double xx,double yy,const std::string&color="0.78 0.81 0.81",double weight=.5){c<<color<<" RG "<<weight<<" w "<<x<<' '<<y<<" m "<<xx<<' '<<yy<<" l S\n";}
 void fill(double x,double y,double w,double h,const std::string&color){c<<color<<" rg "<<x<<' '<<y<<' '<<w<<' '<<h<<" re f\n";}
 void rect(double x,double y,double w,double h,bool board=false){fill(x,y,w,h,board?"0.91 0.87 0.78":"0.96 0.97 0.96");c<<"0.55 0.58 0.56 RG 0.5 w "<<x<<' '<<y<<' '<<w<<' '<<h<<" re S\n";}
 void title(const std::string&s,const std::string&sub=""){text(36,578,"FABRICATION / PLYWOOD",8,"0.31 0.40 0.40",true);text(36,550,s,23);if(!sub.empty())text(36,530,sub,9,"0.40 0.45 0.46");line(36,516,756,516,"0.17 0.32 0.33",1);}
 void section(double y,const std::string&s){text(36,y,s,10,"0.17 0.32 0.33",true);}
 void badge(double x,double y,const std::string&s){fill(x,y,25,13,"0.17 0.32 0.33");text(x+3,y+3,s,8,"1 1 1",true);}
};
struct Document {
 std::vector<std::string> pages;
 void add(Page&p){pages.push_back(p.c.str());}
 void lines(const std::string&title,const std::vector<std::string>&rows){
  for(size_t start=0;start<rows.size()||start==0;start+=29){Page p;p.title(title+(start?" (continued)":""));double y=490;for(size_t i=start;i<std::min(start+29,rows.size());++i,y-=16)p.text(32,y,rows[i]);add(p);}
 }
 void write(const std::filesystem::path&path){
  std::ofstream f(path,std::ios::binary);f.exceptions(std::ios::failbit|std::ios::badbit);
  auto font=helveticaNeue();
  std::vector<std::string> obj(5);obj[0]="<< /Type /Catalog /Pages 2 0 R >>";
  obj[2]="<< /Type /Font /Subtype /TrueType /BaseFont /HelveticaNeue /Encoding /WinAnsiEncoding /FirstChar 32 /LastChar 126 /Widths ["+font.widths+"] /FontDescriptor 4 0 R >>";
  obj[3]="<< /Type /FontDescriptor /FontName /HelveticaNeue /Flags 32 /FontBBox [-1000 -1000 2000 2000] /ItalicAngle 0 /Ascent "+num(font.ascent)+" /Descent "+num(font.descent)+" /CapHeight 714 /StemV 80 /FontFile2 5 0 R >>";
  obj[4]="<< /Length "+std::to_string(font.bytes.size())+" /Length1 "+std::to_string(font.bytes.size())+" >>\nstream\n"+font.bytes+"\nendstream";
  std::string kids;
  for(size_t i=0;i<pages.size();++i){int id=int(obj.size())+1;kids+=std::to_string(id)+" 0 R ";Page foot;foot.line(36,37,756,37);foot.text(36,22,"SHOP PACKET    /    DIMENSIONS IN MM    /    NOT TO SCALE",7);foot.text(696,22,"Page "+std::to_string(i+1)+" / "+std::to_string(pages.size()),8);auto stream=pages[i]+foot.c.str();obj.push_back("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 792 612] /Resources << /Font << /F1 3 0 R >> >> /Contents "+std::to_string(id+1)+" 0 R >>");obj.push_back("<< /Length "+std::to_string(stream.size())+" >>\nstream\n"+stream+"endstream");}
  obj[1]="<< /Type /Pages /Count "+std::to_string(pages.size())+" /Kids ["+kids+"] >>";
  f<<"%PDF-1.4\n";std::vector<std::streamoff> offsets;
  for(size_t i=0;i<obj.size();++i){offsets.push_back(f.tellp());f<<i+1<<" 0 obj\n"<<obj[i]<<"\nendobj\n";}
  auto x=f.tellp();f<<"xref\n0 "<<obj.size()+1<<"\n0000000000 65535 f \n";for(auto o:offsets)f<<std::setfill('0')<<std::setw(10)<<o<<" 00000 n \n";
  f<<"trailer\n<< /Size "<<obj.size()+1<<" /Root 1 0 R >>\nstartxref\n"<<x<<"\n%%EOF\n";
 }
};
inline void wrapped(std::vector<std::string>&rows,const std::string&s,size_t limit=115){for(size_t i=0;i<s.size();i+=limit)rows.push_back((i?"    ":"")+s.substr(i,limit));}
inline std::string shortName(const std::string&s){auto i=s.find_last_of('/');auto n=i==std::string::npos?s:s.substr(i+1);auto instance=n.find_last_of('#');if(instance!=std::string::npos)n=n.substr(0,instance);return n;}
inline void inventory(Document&doc,const std::vector<cut::Part>&parts,const std::vector<cut::Rejected>&rejected){
 for(size_t start=0;start<parts.size();start+=17){Page p;p.title("Parts register", "One row per physical blank. P IDs link this register to layouts and labels.");
  p.fill(36,481,720,22,"0.17 0.32 0.33");
  for(auto a:std::vector<std::pair<double,std::string>>{{45,"ID"},{88,"PART / MODEL BOARD NAME"},{416,"LENGTH"},{486,"WIDTH"},{551,"THICK."},{615,"QTY"},{654,"FINISHING"}})p.text(a.first,489,a.second,8,"1 1 1",true);
  double y=461;for(size_t i=start;i<std::min(start+17,parts.size());++i,y-=21){auto&a=parts[i];if((i-start)%2==0)p.fill(36,y-6,720,21,"0.96 0.97 0.96");p.text(45,y,pid(i+1),9,"0.17 0.32 0.33",true);p.text(88,y,fit(shortName(a.name),310,9),9);p.right(459,y,num(a.w),9);p.right(529,y,num(a.h),9);p.right(591,y,num(a.t),9);p.text(624,y,"1",9);p.text(654,y,a.machined?"Machine *":"Blank",8.5);p.line(36,y-6,756,y-6);}
  p.text(36,77,"* Additional machining required: dados, holes or other material removal detected.",9,"0.17 0.32 0.33",true);
  p.text(36,61,"Machining sizes are not extracted. Use source drawings / model for positions, depths and drilling setups.",9);doc.add(p);
 }
 std::vector<std::string> rows;for(auto&r:rejected)if(!r.excluded)wrapped(rows,"UNPLANNED: "+r.name+" - "+r.reason);if(!rows.empty())doc.lines("Exceptions / unplanned objects",rows);
}
struct StockRow {std::string id,stock;double thickness,w,h;int cuts,parts;};
inline void summary(Document&doc,const std::vector<StockRow>&stocks,size_t parts,size_t hardware,size_t unsupported,double kerf,bool rotate,const std::string&workflow,const std::vector<std::filesystem::path>&inputs){
 Page p;p.title("Plywood cutting plan","Production overview  /  Rectangular blanks first, machining second");
 int cuts=0;for(auto&s:stocks)cuts+=s.cuts;
 std::vector<std::pair<std::string,std::string>> metrics={{std::to_string(stocks.size()),"SHEETS TO BUY"},{std::to_string(parts),"INDIVIDUAL BLANKS"},{std::to_string(cuts),"SAW CUTS"},{num(kerf)+" mm","MAX. KERF ALLOWANCE"}};
 for(int i=0;i<4;++i){double x=36+i*184;p.fill(x,431,168,67,"0.94 0.96 0.95");p.text(x+12,461,metrics[i].first,25);p.text(x+12,444,metrics[i].second,8,"0.31 0.40 0.40",true);}
 p.section(405,"01   STOCK TO BUY");p.fill(36,373,720,22,"0.17 0.32 0.33");
 for(auto a:std::vector<std::pair<double,std::string>>{{46,"SHEET"},{140,"PLYWOOD"},{260,"STOCK SIZE"},{410,"USABLE SIZE (MM)"},{618,"BLANKS"},{703,"CUTS"}})p.text(a.first,381,a.second,8,"1 1 1",true);
 double y=352;size_t shown=std::min(size_t(4),stocks.size());for(size_t i=0;i<shown;++i,y-=26){auto&s=stocks[i];p.text(46,y,s.id,10);p.text(140,y,num(s.thickness)+" mm",10);p.text(260,y,s.stock+" ft",10);p.text(410,y,num(s.w)+" x "+num(s.h),10);p.text(631,y,std::to_string(s.parts),10);p.text(711,y,std::to_string(s.cuts),10);p.line(36,y-9,756,y-9);}
 if(stocks.size()>shown)p.text(36,y,"Additional sheets listed on continuation page.",9);
 double by=stocks.size()>2?211:253;p.section(by,"02   CUTTING WORKFLOW");
 p.text(36,by-23,"1",12,"0.17 0.32 0.33",true);p.text(55,by-23,"Follow FROM and KEEP LEFT for each cut; see the cutting guide on the next page.",10);
 p.text(36,by-43,"2",12,"0.17 0.32 0.33",true);p.text(55,by-43,"Mark every blank with its P ID before separating similar parts.",10);
 p.text(36,by-63,"3",12,"0.17 0.32 0.33",true);p.text(55,by-63,"Machine flagged parts using the source model / drawings before assembly.",10);
 p.fill(36,by-146,720,60,"0.96 0.97 0.96");
 p.text(48,by-103,rotate?"Grain: rotation allowed. Verify face and grain before cutting.":"Grain: longest blank edge follows sheet X. Preserve the layout when turning panels.",9);
 p.text(48,by-119,"Usable stock sizes assumed; no edge-trim allowance. No stacked cuts assumed.",9);
 p.text(48,by-135,"Material / grade assumed uniform; sheets grouped by thickness. "+std::to_string(hardware)+" hardware objects excluded.",9);
 p.text(36,65,unsupported?"PARTIAL PLAN: "+std::to_string(unsupported)+" unsupported objects; see exceptions.":"Plan: "+workflow+" workflow. Search is heuristic; minimum cut count is not proven.",8,"0.40 0.45 0.46");
 doc.add(p);
 if(stocks.size()>shown){std::vector<std::string>extra;for(size_t i=shown;i<stocks.size();++i){auto&s=stocks[i];extra.push_back(s.id+"   "+num(s.thickness)+" mm   "+s.stock+"   "+num(s.w)+" x "+num(s.h)+" mm");}doc.lines("Stock to buy / continued",extra);}
 std::vector<std::string>sources;for(auto&input:inputs)wrapped(sources,input.filename().string());
 // Source identity lives in the overview margin when short; otherwise preserve it on a continuation page.
 if(sources.size()==1){Page source;source.text(36,50,"SOURCE  "+fit(sources.front(),665,7),7,"0.40 0.45 0.46");doc.pages.front()+=source.c.str();}else if(!sources.empty())doc.lines("Source models",sources);
}
inline void cuttingGuide(Document&doc,double kerf){
 Page p;p.title("Keep the sized piece on the left", "Use local measurements for every cut / Diagram positions use the maximum kerf allowance");
 p.section(490,"01   PICK THE CORRECT SUBPANEL");
 p.text(36,470,"FROM identifies the piece: S1 = this group's sheet 1; C4L = left piece from cut 4; C4R = right remainder.",10);
 p.text(36,452,"After each cut, label both sides CnL / CnR and retain any remainder referenced by a later cut.",10);
 p.section(423,"02   ORIENT IT BEFORE MEASURING");
 p.text(36,403,"V: put the diagram-left edge on your left. H: turn clockwise so the diagram-bottom edge is on your left.",10);
 p.text(36,385,"Use the sheet diagram as the orientation reference each time. Turning a panel does not change its grain layout.",10);
 p.section(356,"03   MEASURE KEEP LEFT TO THE NEAREST CUTTING EDGE");
 p.text(36,336,"Measure from the current left reference edge to the blade tooth edge nearest the kept piece, not the blade centre.",10);
 p.text(36,318,"Reset this measurement for every cut. Do not use cumulative sheet coordinates or subtract the kerf.",10);
 p.rect(92,211,280,79,true);p.text(155,247,"KEEP LEFT",17,"0.17 0.32 0.33",true);
 p.fill(372,211,8,79,"0.65 0.34 0.16");p.rect(380,211,290,79);p.text(431,247,"RIGHT REMAINDER",13);
 p.line(92,199,372,199,"0.17 0.32 0.33",1);p.line(92,194,92,204);p.line(372,194,372,204);
 p.text(150,180,"KEEP LEFT dimension",11,"0.17 0.32 0.33",true);
 p.text(393,180,"Kerf is taken from this side",10,"0.65 0.34 0.16");
 p.section(147,"04   FINISH REMAINDERS TO SIZE");
 p.text(36,127,"The plan reserves "+num(kerf)+" mm per cut. Smaller actual kerfs leave extra stock; finishing cuts remove it.",10);
 p.text(36,109,"Finish even at a diagram edge. Skip a pass only if the piece measures to size; still use its CnL label.",10);
 p.text(36,91,"The actual kerf must be no greater than "+num(kerf)+" mm. Replan for a wider blade. Exact stock size and accurate setup still matter.",10);
 p.text(36,65,"P IDs in LEFT PIECE mark finished blanks. Other left pieces and right remainders may need further cuts.",9,"0.40 0.45 0.46");
 doc.add(p);
}
inline void labels(Document&doc,const std::vector<cut::Part>&parts){
 for(size_t start=0;start<parts.size();start+=10){Page p;p.title("Blank identification labels","Cut along the outlines and attach each label immediately after cutting. Print at actual size.");
 for(size_t i=start;i<std::min(start+10,parts.size());++i){auto&a=parts[i];double x=36+((i-start)%2)*366,y=416-((i-start)/2)*88;p.rect(x,y,354,80);p.fill(x,y+49,354,31,"0.94 0.96 0.95");p.text(x+12,y+57,pid(i+1),18,"0.17 0.32 0.33",true);p.text(x+67,y+60,num(a.w)+" x "+num(a.h)+" x "+num(a.t)+" mm",12);p.text(x+12,y+31,fit(shortName(a.name),330,10),10);p.text(x+12,y+12,a.machined?"MACHINE AFTER CUTTING  /  See source drawing":"RECTANGULAR BLANK",8,"0.40 0.45 0.46");}doc.add(p);}
}
inline void sheet(Document&doc,const cut::Stock&stock,double thickness,int sheetIndex,int group,const std::vector<Placement>&places,const std::vector<int>&ids,const std::vector<SawCut>&cuts,double kerf){
 std::vector<SawCut>selected;for(auto&c:cuts)if(c.sheet==sheetIndex)selected.push_back(c);
 Page p;std::string ref=std::to_string(group)+"."+std::to_string(sheetIndex);p.title("Sheet "+ref+"  /  "+num(thickness)+" mm plywood",stock.name+" ft stock  /  "+num(stock.w)+" x "+num(stock.h)+" mm  /  "+std::to_string(selected.size())+" saw cuts");
 double scale=std::min(400/stock.w,400/stock.h),ox=48,oy=68;
 p.text(48,494,"CUTTING LAYOUT",9,"0.17 0.32 0.33",true);p.text(48,479,"P = part ID     C = saw cut     Grey = remaining stock",8,"0.40 0.45 0.46");
 p.rect(ox,oy,stock.w*scale,stock.h*scale);
 for(auto&a:places)if(a.sheet==sheetIndex){double x=ox+a.x*scale,y=oy+a.y*scale,w=a.w*scale,h=a.h*scale;p.rect(x,y,w,h,true);auto id=pid(ids[a.id]);
  auto dims=num(a.w)+" x "+num(a.h);
  if(h>=24&&w>=std::max(width(id,10),width(dims,8))+12){
   bool compact=h<40;
   p.text(x+6,y+h-(compact?11:14),id,compact?9:10,"0.17 0.32 0.33",true);
   p.text(x+6,y+h-(compact?21:27),dims,8);
  }else{
   // Narrow strips use one line along their long edge; never omit dimensions.
   auto caption=id+"  "+dims;bool vertical=h>w;
   double along=vertical?h:w,across=vertical?w:h;
   double inset=std::min(3.0,across*.15);
   double size=std::min({8.0,(along-2*inset)/width(caption,1),across-2*inset});
   if(vertical){p.c<<"q 0 1 -1 0 "<<x+(w+size*.7)/2<<' '<<y+inset<<" cm\n";p.text(0,0,caption,size);p.c<<"Q\n";}
   else p.text(x+inset,y+(h-size*.7)/2,caption,size);
  }
 }
 // Cut badges sit near the far edge of each cut, away from the part identifiers.
 for(auto&c:selected){double x=c.axis=='V'?c.at:c.x,y=c.axis=='H'?c.at:c.y,xx=c.axis=='V'?c.at:c.x+c.w,yy=c.axis=='H'?c.at:c.y+c.h;
  std::string color=c.stage==1?"0.16 0.40 0.56":c.stage==2?"0.65 0.34 0.16":"0.42 0.31 0.52";
  p.c<<"q [3 2] 0 d\n";p.line(ox+x*scale,oy+y*scale,ox+xx*scale,oy+yy*scale,color,.7);p.c<<"Q\n";
  double cx=ox+(c.axis=='V'?x:xx)*scale,cy=oy+(c.axis=='V'?yy:y)*scale;
  std::string label="C"+std::to_string(c.sequence);double bw=width(label,7)+5;
  // Keep badge inside the far end of its own subpanel.
  p.fill(cx-bw-1,cy-10,bw,10,"1 1 1");p.text(cx-bw+1,cy-7,label,7,color,true);
 }
 double right=ox+stock.w*scale;p.line(ox,54,right,54,"0.17 0.32 0.33",.8);p.line(right-5,57,right,54,"0.17 0.32 0.33",.8);p.line(right-5,51,right,54,"0.17 0.32 0.33",.8);p.fill(ox+8,49,111,11,"1 1 1");p.text(ox+11,52,"SHEET X / GRAIN REFERENCE",6.5,"0.17 0.32 0.33");
 double sx=490;p.text(sx,494,"SAW ORDER / KEEP LEFT",9,"0.17 0.32 0.33",true);p.text(sx,479,"Label both sides CnL / CnR. P = finished blank.",8,"0.40 0.45 0.46");p.fill(sx,452,266,19,"0.17 0.32 0.33");
 p.text(sx+5,458,"CUT",7,"1 1 1",true);p.text(sx+38,458,"FROM",7,"1 1 1",true);p.text(sx+88,458,"AXIS",7,"1 1 1",true);p.text(sx+123,458,"KEEP LEFT",7,"1 1 1",true);p.text(sx+187,458,"LEFT PIECE",7,"1 1 1",true);
 int row=0;std::vector<std::string>remaining;
 for(auto&c:selected){
  auto keep=num(c.at-(c.axis=='V'?c.x:c.y));auto left="C"+std::to_string(c.sequence)+"L";
  auto finished=std::find_if(places.begin(),places.end(),[&](const Placement&a){return a.sheet==sheetIndex&&a.source==left;});
  auto output=finished==places.end()?left:pid(ids[finished->id])+" / "+left;
  if(row<19){double y=437-row*16;if(row%2==0)p.fill(sx,y-5,266,16,"0.96 0.97 0.96");
   p.text(sx+5,y,"C"+std::to_string(c.sequence),8,"0.17 0.32 0.33",true);p.text(sx+38,y,c.source,8);p.text(sx+95,y,std::string(1,c.axis),8);p.right(sx+176,y,keep,8);p.text(sx+187,y,output,7.5);p.line(sx,y-5,756,y-5);
  }else remaining.push_back("C"+std::to_string(c.sequence)+"   FROM "+c.source+"   "+c.axis+"   KEEP LEFT "+keep+" mm   -> "+output);
  ++row;
 }
 p.fill(sx,68,266,58,"0.94 0.96 0.95");p.text(sx+9,110,"V: left edge on left. H: turn bottom edge to left.",8);p.text(sx+9,95,"Measure afresh to the blade's kept-side edge.",8);p.text(sx+9,80,"Max kerf "+num(kerf)+" mm. See cutting guide.",8);doc.add(p);
 if(!remaining.empty())doc.lines("Saw order / sheet "+ref+" continued",remaining);
}
}

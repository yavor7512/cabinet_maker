#include "model.hpp"
#include "shop.hpp"
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <set>
using namespace cut;namespace fs=std::filesystem;
std::string csv(const std::string&s){std::string r="\"";for(char c:s){if(c=='"')r+='"';r+=c;}return r+'"';}
std::string xml(const std::string&s){std::string r;for(char c:s){if(c=='&')r+="&amp;";else if(c=='<')r+="&lt;";else if(c=='>')r+="&gt;";else if(c=='"')r+="&quot;";else r+=c;}return r;}
#include "saw.hpp"
#include "packet.hpp"
void help(){std::cout<<R"(NAME
    step-cutlist - inventory rectangular STEP boards and optimize plywood cuts
SYNOPSIS
    step-cutlist --out DIRECTORY [OPTIONS] input.step [more.step ...]
OPTIONS
    --labels             Append printable cut-out part labels to the PDF.
    --export-details     Also retain CSV, SVG and diagnostic files in details/.
    --workflow MODE      shop (default): fewer cuts and grouped strip operations;
                         material: original sheet-area optimization.
    --extra-area-percent N  Shop area budget above material result (default 35).
    --shop-attempts N     Deterministic strip search passes (default 64).
    --sheets LIST        Comma-separated 8x4,4x4,5x5; default 5x5.
                         Exact feet: 2438.4x1219.2,1219.2x1219.2,1524x1524 mm.
    --kerf MM            Maximum planning kerf (default 5).
    --allow-rotate       Permit 90-degree in-sheet rotation (ignore grain).
                         Default locks each part's LONGEST edge to sheet X.
    --max-thickness MM   Sheet-like thickness limit (default 50.8).
    --max-ratio NUMBER   Thickness / smaller face dimension limit (default .5).
    --tolerance MM       Thickness grouping / minimum thickness (default .01).
    --exact-limit N      Exact search up to N parts per thickness (default 12,
                         maximum 16). Above this, use a valid shelf heuristic.
    --seconds N          Exact-search time budget per thickness (default 10).
    --max-candidates N   Exact-search work limit (default 5000000).
    --strict-rectangles  Reject machined boards instead of recovering blanks.
    --skip-unsupported   Explicitly optimize only accepted solids; report rejects.
    --out DIRECTORY      New or empty output directory (default cut-list).
    --                   Remaining arguments are STEP input filenames.
    -h, --help           Print this manual.
GEOMETRY
    Arbitrarily positioned/rotated closed rectangular solids are accepted.
    Smallest dimension is thickness. Names and repeated assembly instances are
    retained. Machined boards use stock blanks identified by six surviving
    exterior datum planes. Machining is flagged separately. Named hardware
    is excluded; unidentifiable solids are rejected. No machining toolpaths. STEP reader converts source units to millimetres.
OPTIMIZATION
    Groups by thickness. All inputs are assumed to be the SAME material species
    and grade; run separately for different materials. Grain cannot be inferred
    from STEP, so longest-edge alignment is an explicit default assumption.
    Material workflow: least purchased sheet AREA, then fewest sheets, with unlimited
    stock of the selected sizes. This is not price optimization.
    Exact subset dynamic programming searches rectangular guillotine layouts
    (full straight cuts through each current subpanel), with kerf. If exact
    search exceeds a limit, output is labeled heuristic, not proven optimal.
    Shop workflow is heuristic: actual cut count first, then slicing-direction
    changes and area, within the material-area budget. Shared waste is removed
    before subdivision. Rip/crosscut strip layouts are searched in both directions.
    Remainders receive finishing cuts so blank sizes do not depend on exact kerf.
    Actual kerf must not exceed the planning allowance; smaller kerfs leave waste.
    No edge trim allowance; stock sizes must be usable, square dimensions.
OUTPUT
    shop-packet.pdf      Default: summary, parts, machining flags, cutting layouts.
    With --export-details, the following are retained under details/:
    inventory.csv       Board blanks, dimensions and machining flags.
    machining.csv       Parts requiring work after rectangular blank cutting.
    excluded.csv        Named screws/bolts/nuts/washers omitted from plywood.
    quantities.csv      Aggregated dimensions and quantities.
    rejected.csv        Unsupported objects and reasons (always written).
    report.txt          Stock usage, utilization and optimization status.
    thickness-N/        placements.csv, cuts.csv, sheet-N.svg for each thickness.
    Cut coordinates use sheet lower-left origin, +X right, +Y up. A cut position
    is the kept edge; kerf falls on its positive side. Follow the FROM subpanel
    and KEEP LEFT distance, not absolute diagram coordinates. For H cuts turn
    the subpanel clockwise so its diagram-bottom edge is on the left.
    CnL is the kept piece after cut n; CnR is its right remainder. S1 is sheet 1.
    Edge finishing passes may remove nothing with the full planning kerf.
EXIT STATUS
    0 valid layout (check report for optimality); 1 argument/import/layout failure;
    2 rejected objects prevented planning. Inventory is still written.
)";}
double number(const std::string&s){std::size_t n;double x=std::stod(s,&n);if(n!=s.size()||!std::isfinite(x))throw std::runtime_error("Expected finite numeric value");return x;}
int main(int argc,char**argv){try{
  Settings cfg;GeometrySettings geo;std::vector<fs::path>inputs;fs::path out="cut-list";bool skip=false,positional=false,exportDetails=false,printLabels=false;std::string sheetList="5x5",workflow="shop";double extraArea=35;int shopAttempts=64;
  for(int i=1;i<argc;++i){std::string a=argv[i];if(!positional&&(a=="--help"||a=="-h")){help();return 0;}
    if(!positional&&a=="--"){positional=true;continue;}
    if(!positional&&a=="--labels"){printLabels=true;continue;}
    if(!positional&&a=="--export-details"){exportDetails=true;continue;}
    if(!positional&&a=="--strict-rectangles"){geo.strict=true;continue;}
    if(!positional&&a=="--allow-rotate"){cfg.rotate=true;continue;}
    if(!positional&&a=="--skip-unsupported"){skip=true;continue;}
    if(!positional&&a.rfind("--",0)==0){if(++i==argc)throw std::runtime_error("Missing value for "+a);std::string v=argv[i];
      if(a=="--workflow"){if(v!="shop"&&v!="material")throw std::runtime_error("Workflow must be shop or material");workflow=v;}
      else if(a=="--extra-area-percent"){extraArea=number(v);if(extraArea<0||extraArea>100)throw std::runtime_error("Extra area must be 0..100 percent");}
      else if(a=="--shop-attempts"){double n=number(v);if(n<1||n>1000||n!=std::floor(n))throw std::runtime_error("Shop attempts must be integer 1..1000");shopAttempts=(int)n;}
      else if(a=="--out")out=v;else if(a=="--sheets")sheetList=v;else if(a=="--kerf")cfg.kerf=number(v);else if(a=="--max-thickness")geo.maxThickness=number(v);else if(a=="--max-ratio")geo.maxRatio=number(v);else if(a=="--tolerance")geo.tolerance=number(v);else if(a=="--seconds")cfg.seconds=number(v);
      else if(a=="--exact-limit"){double n=number(v);if(n<0||n>16||n!=std::floor(n))throw std::runtime_error("Exact limit must be an integer 0..16");cfg.exactLimit=(int)n;}
      else if(a=="--max-candidates"){double n=number(v);if(n<1||n>1e9||n!=std::floor(n))throw std::runtime_error("Candidate limit must be integer 1..1000000000");cfg.maxCandidates=(std::uint64_t)n;}
      else throw std::runtime_error("Unknown option "+a);continue;
    }inputs.push_back(a);
  }
  if(inputs.empty()||out.empty()||cfg.kerf<0||cfg.seconds<=0||geo.tolerance<=0||geo.maxThickness<=geo.tolerance||geo.maxRatio<=0||geo.maxRatio>1)throw std::runtime_error("Invalid parameters or no STEP inputs; see --help");
  std::vector<Stock>stocks;std::set<std::string>unique;std::istringstream ls(sheetList);std::string token;
  while(std::getline(ls,token,',')){if(!unique.insert(token).second)continue;if(token=="8x4")stocks.push_back({token,2438.4,1219.2});else if(token=="4x4")stocks.push_back({token,1219.2,1219.2});else if(token=="5x5")stocks.push_back({token,1524,1524});else throw std::runtime_error("Unknown sheet size "+token);}
  if(stocks.empty())throw std::runtime_error("No stock sizes selected");
  if(fs::exists(out)&&!fs::is_empty(out))throw std::runtime_error("Output directory must be new or empty");
  std::vector<Part>parts;std::vector<Rejected>rejected;for(auto&p:inputs)load(p,geo,parts,rejected);
  const auto packetRoot=out;out/="details";
  fs::create_directories(out);
  packet::Document document;std::vector<packet::StockRow> stockRows;
  packet::inventory(document,parts,rejected);
  auto file=[&](fs::path p){std::ofstream f(p);f.exceptions(std::ios::badbit|std::ios::failbit);f<<std::setprecision(12);return f;};
  auto inventory=file(out/"inventory.csv");inventory<<"id,name,length_mm,width_mm,thickness_mm,machining_required,removed_volume_mm3\n";
  for(std::size_t i=0;i<parts.size();++i){auto&p=parts[i];inventory<<i+1<<','<<csv(p.name)<<','<<p.w<<','<<p.h<<','<<p.t<<','<<p.machined<<','<<p.removedVolume<<'\n';}
  auto rejects=file(out/"rejected.csv");rejects<<"name,reason\n";for(auto&r:rejected)if(!r.excluded)rejects<<csv(r.name)<<','<<csv(r.reason)<<'\n';
  auto excluded=file(out/"excluded.csv");excluded<<"name,reason\n";std::size_t unsupported=0,hardwareCount=0;
  for(auto&r:rejected){if(r.excluded){++hardwareCount;excluded<<csv(r.name)<<','<<csv(r.reason)<<'\n';}else ++unsupported;}
  auto machining=file(out/"machining.csv");machining<<"inventory_id,name,note,removed_volume_mm3\n";
  for(std::size_t i=0;i<parts.size();++i)if(parts[i].machined)machining<<i+1<<','<<csv(parts[i].name)<<",Additional machining required; refer to original STEP,"<<parts[i].removedVolume<<'\n';
  // Quantities are display-rounded only; optimizer always retains measured sizes.
  std::map<std::string,int>counts;for(auto&p:parts){std::ostringstream key;key<<std::fixed<<std::setprecision(3)<<p.w<<','<<p.h<<','<<p.t;counts[key.str()]++;}
  auto quantities=file(out/"quantities.csv");quantities<<"length_mm,width_mm,thickness_mm,quantity\n";for(auto&[k,n]:counts)quantities<<k<<','<<n<<'\n';
  auto report=file(out/"report.txt");report<<"Accepted solids: "<<parts.size()<<"\nUnsupported objects: "<<unsupported<<"\nExcluded hardware: "<<hardwareCount<<"\nKerf: "<<cfg.kerf<<" mm\nGrain policy: "<<(cfg.rotate?"rotation permitted":"longest part edge parallel to sheet X")<<"\n";
  if(unsupported&&!skip){report<<"STOPPED: unsupported geometry. No cutting plan generated. See rejected.csv.\n";packet::Document stopped;stopped.lines("STOPPED - unsupported geometry",{"No cutting layouts generated. Review the exceptions before proceeding."});stopped.pages.insert(stopped.pages.end(),document.pages.begin(),document.pages.end());stopped.write(packetRoot/"shop-packet.pdf");if(!exportDetails)fs::remove_all(out);std::cerr<<"Unsupported objects found; exceptions listed in packet at "<<packetRoot<<". Use --skip-unsupported only to explicitly plan the accepted subset.\n";return 2;}
  if(parts.empty())throw std::runtime_error("No rectangular sheet parts found");
  struct Group{double t;std::vector<Part>parts;std::vector<int>ids;};std::vector<Group>groups;
  std::vector<int>order;for(int i=0;i<(int)parts.size();++i)order.push_back(i);
  std::sort(order.begin(),order.end(),[&](int a,int b){return parts[a].t<parts[b].t;});
  for(int id:order){auto&p=parts[id];if(groups.empty()||p.t-groups.back().t>geo.tolerance)groups.push_back({p.t,{},{}});groups.back().parts.push_back(p);groups.back().ids.push_back(id+1);}
  int gindex=0;for(auto&g:groups){auto result=workflow=="shop"?optimizeShop(g.parts,stocks,cfg,extraArea,shopAttempts):optimize(g.parts,stocks,cfg);auto dir=out/("thickness-"+std::to_string(++gindex));fs::create_directories(dir);
    auto cuts=file(dir/"cuts.csv");cuts<<"sheet,sequence,axis,subpanel_x,subpanel_y,subpanel_width,subpanel_height,cut_coordinate,cut_length,stage,from_panel,keep_left_mm,left_piece,right_remainder,orientation\n";
    std::vector<Placement>places;int sequence=0,sheet=0;double used=0;for(auto&p:g.parts)used+=p.w*p.h;
    std::vector<SawCut>sawCuts;
    for(auto&s:result.sheets)shopLayout(s.tree,++sheet,0,0,s.stock.w,s.stock.h,cfg.kerf,places,sawCuts,0,0,
      workflow=="shop"?planMode(s.tree,s.stock.w,s.stock.h,cfg.kerf):1);
    if(workflow=="shop")std::stable_sort(sawCuts.begin(),sawCuts.end(),[](const SawCut&a,const SawCut&b){return a.sheet!=b.sheet?a.sheet<b.sheet:a.stage<b.stage;});
    labelSawSequence(sawCuts,result.sheets,cfg.kerf,places);
    for(auto&c:sawCuts){sequence=c.sequence;cuts<<c.sheet<<','<<c.sequence<<','<<c.axis<<','<<c.x<<','<<c.y<<','<<c.w<<','<<c.h<<','<<c.at<<','<<c.length<<','<<c.stage<<','<<c.source<<','<<c.at-(c.axis=='V'?c.x:c.y)<<",C"<<c.sequence<<"L,C"<<c.sequence<<"R,"<<(c.axis=='V'?"diagram left edge on left":"turn clockwise: diagram bottom edge on left")<<'\n';}
    if(workflow=="shop"&&(int)sawCuts.size()!=shopCuts(result))throw std::runtime_error("Cut-count score differs from emitted saw sequence");
    report<<"\nWorkflow: "<<workflow<<"; actual saw cuts: "<<sequence<<"\n";

    validate(places,g.parts,result.sheets,cfg.kerf);
    auto placements=file(dir/"placements.csv");placements<<"sheet,stock,inventory_id,x_mm,y_mm,width_mm,height_mm,rotated,finished_piece\n";
    for(auto&p:places)placements<<p.sheet<<','<<result.sheets[p.sheet-1].stock.name<<','<<g.ids[p.id]<<','<<p.x<<','<<p.y<<','<<p.w<<','<<p.h<<','<<p.rotated<<','<<p.source<<'\n';
    report<<"\nThickness group "<<gindex<<": "<<g.t<<" mm, "<<g.parts.size()<<" parts\nStatus: "<<(result.exact?"OPTIMAL (guillotine model)":"HEURISTIC, optimum not proven")<<"\n"<<result.reason<<"\nCandidates: "<<result.candidates<<"\nSheets: "<<result.sheets.size()<<"; purchased area "<<area(result.sheets)/1e6<<" m2; utilization "<<100*used/area(result.sheets)<<"% (remainder includes kerf)\n";
    for(int i=0;i<(int)result.sheets.size();++i){auto&s=result.sheets[i];
      packet::sheet(document,s.stock,g.t,i+1,gindex,places,g.ids,sawCuts,cfg.kerf);
      int sheetCuts=0,sheetParts=0;for(auto&c:sawCuts)if(c.sheet==i+1)++sheetCuts;for(auto&p:places)if(p.sheet==i+1)++sheetParts;
      stockRows.push_back({std::to_string(gindex)+"."+std::to_string(i+1),s.stock.name,g.t,s.stock.w,s.stock.h,sheetCuts,sheetParts});
      report<<"  Sheet "<<i+1<<": "<<s.stock.name<<'\n';
      auto svg=file(dir/("sheet-"+std::to_string(i+1)+".svg"));svg<<"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"-30 -65 "<<s.stock.w+850<<' '<<std::max(s.stock.h+95,static_cast<double>(sawCuts.size()*32+150))<<"\"><rect x=\"0\" y=\"0\" width=\""<<s.stock.w<<"\" height=\""<<s.stock.h<<"\" fill=\"#eee\" stroke=\"#222\"/><text x=\"0\" y=\"-20\" font-size=\"24\">"<<s.stock.name<<" | "<<g.t<<" mm | sheet "<<i+1<<" | sheet X = grain reference</text>";
      for(auto&p:places)if(p.sheet==i+1){double sy=s.stock.h-p.y-p.h;svg<<"<g><title>"<<xml(g.parts[p.id].name)<<"</title><rect x=\""<<p.x<<"\" y=\""<<sy<<"\" width=\""<<p.w<<"\" height=\""<<p.h<<"\" fill=\"#d4ad73\" stroke=\"#49331c\"/><text x=\""<<p.x+5<<"\" y=\""<<sy+20<<"\" font-size=\"16\">#"<<g.ids[p.id]<<" "<<p.w<<" x "<<p.h<<"</text></g>";}
      {
        const double sidebar=s.stock.w+80;int row=0;
        svg<<"<text x=\""<<sidebar<<"\" y=\"0\" font-size=\"24\">SAW ORDER (mm)</text><text x=\""<<sidebar<<"\" y=\"32\" font-size=\"18\">V: diagram left on left; H: turn bottom to left</text>";
        for(auto&c:sawCuts)if(c.sheet==i+1){
          const char*color=c.stage==1?"#1565c0":c.stage==2?"#c62828":"#6a1b9a";
          double x1=c.axis=='V'?c.at:c.x,x2=c.axis=='V'?c.at:c.x+c.w;
          double y1=s.stock.h-(c.axis=='H'?c.at:c.y),y2=s.stock.h-(c.axis=='H'?c.at:c.y+c.h);
          svg<<"<line x1=\""<<x1<<"\" y1=\""<<y1<<"\" x2=\""<<x2<<"\" y2=\""<<y2<<"\" stroke=\""<<color<<"\" stroke-width=\"2\" stroke-dasharray=\"9 5\"/>"
             <<"<circle cx=\""<<(x1+x2)/2<<"\" cy=\""<<(y1+y2)/2<<"\" r=\"15\" fill=\"white\" stroke=\""<<color<<"\"/><text text-anchor=\"middle\" x=\""<<(x1+x2)/2<<"\" y=\""<<(y1+y2)/2+6<<"\" font-size=\"18\" fill=\""<<color<<"\">"<<c.sequence<<"</text>";
          svg<<"<text x=\""<<sidebar<<"\" y=\""<<70+32*row++<<"\" font-size=\"19\" fill=\""<<color<<"\">"<<c.sequence<<". Stage "<<c.stage<<" | "<<c.axis<<" | from "<<c.source<<" | KEEP LEFT "<<c.at-(c.axis=='V'?c.x:c.y)<<" | panel "<<c.w<<" x "<<c.h<<"</text>";
        }
        svg<<"<text x=\""<<sidebar<<"\" y=\""<<100+32*row<<"\" font-size=\"18\">Kerf "<<cfg.kerf<<" max; measure each cut from its current kept edge.</text>";
      }
      svg<<"</svg>\n";

    }
  }
  report<<"\nKEEP LEFT: measure from the current subpanel reference edge to the nearest cutting edge.\nFROM labels track left pieces (CnL) and right remainders (CnR); S1 is sheet 1.\nV: diagram left edge on left. H: turn clockwise so diagram bottom edge is on left.\nNominal sheet coordinates and remainder sizes assume the maximum planning kerf.\nFinished dimensions verified at zero, half and full planning kerf; actual kerf must not exceed allowance.\nFinishing passes at nominal outer edges can be skipped only after measuring the finished dimension.\n";
  report<<"\nStock dimensions are usable sizes with no edge trimming. All inputs assumed same material/grade.\n";
  if(printLabels)packet::labels(document,parts);
  packet::Document front;
  packet::summary(front,stockRows,parts.size(),hardwareCount,unsupported,cfg.kerf,cfg.rotate,workflow,inputs);packet::cuttingGuide(front,cfg.kerf);front.pages.insert(front.pages.end(),document.pages.begin(),document.pages.end());front.write(packetRoot/"shop-packet.pdf");
  if(!exportDetails)fs::remove_all(out);
  std::cout<<"Verified shop packet: "<<fs::absolute(packetRoot/"shop-packet.pdf")<<'\n';return 0;
}catch(const Standard_Failure&e){std::cerr<<e.GetMessageString()<<'\n';return 1;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

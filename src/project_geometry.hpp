#pragma once
#include "geometry.hpp"
#include "design.hpp"
#include "temporary_step.hpp"
#include <BRepExtrema_DistShapeShape.hxx>

namespace design {
inline TopoDS_Shape boxShape(const Box& b) {
  return BRepPrimAPI_MakeBox(gp_Pnt(b.low[0],b.low[1],b.low[2]),b.size[0],b.size[1],b.size[2]).Shape();
}
struct Generated {
  std::vector<TopoDS_Shape> parts;
  std::vector<std::string> names;
  std::vector<part_role::Role> roles;
  void add(const TopoDS_Shape& shape,const std::string& name,part_role::Role role) {
    parts.push_back(shape); names.push_back(name); roles.push_back(role);
  }
  void append(const Generated& other) {
    parts.insert(parts.end(),other.parts.begin(),other.parts.end());
    names.insert(names.end(),other.names.begin(),other.names.end());
    roles.insert(roles.end(),other.roles.begin(),other.roles.end());
  }
};
struct ScrewProfile {
  double length,diameter,headDiameter,headHeight,tipLength,pocketDiameter,seatDistance,edgeClearance,tipClearance;
};
inline ScrewProfile profile(Project& project,const Json& v,const std::string& name) {
  keys(v,{"length","diameter","head_diameter","head_height","tip_length","pocket_diameter","seat_distance","edge_clearance","tip_clearance"},name);
  auto number=[&](const char* key,bool zero=false) {
    double n=project.eval(required(v,key,name));
    if(n<(zero?0:checks.minFeature) || n>checks.maxDimension) throw std::runtime_error(name+"."+key+": invalid dimension");
    return n;
  };
  ScrewProfile p{number("length"),number("diameter"),number("head_diameter"),number("head_height"),number("tip_length"),number("pocket_diameter"),number("seat_distance"),number("edge_clearance",true),number("tip_clearance",true)};
  if(p.length<=p.seatDistance+p.tipLength || p.pocketDiameter<=p.headDiameter || p.headDiameter<=p.diameter || p.headHeight>=p.seatDistance)
    throw std::runtime_error(name+": invalid screw/pocket proportions");
  return p;
}
inline double separation(const TopoDS_Shape& a,const TopoDS_Shape& b) {
  BRepExtrema_DistShapeShape distance(a,b);
  if(!distance.IsDone()) throw std::runtime_error("Could not measure joinery clearance");
  return distance.Value();
}
struct Pocket {
  TopoDS_Shape shape;
  double edgeClearance;
  Bnd_Box bounds;
  Pocket(const TopoDS_Shape& machinedShape,double clearance):shape(machinedShape),edgeClearance(clearance) {
    // Cache conservative bounds of the removed material, including OCCT shape
    // tolerances. Do not depend on a potentially coarser display triangulation.
    if(!shape.IsNull()) BRepBndLib::AddOptimal(shape,bounds,false,true);
  }
  bool tooCloseTo(const Pocket& other) const {
    double clearance=std::max(edgeClearance,other.edgeClearance);
    // Box distance is a lower bound on shape distance. The numerical margin
    // only controls pruning; the exact clearance rule below stays unchanged.
    // Empty bounds must fall through to the normal failed-measurement error.
    if(!bounds.IsVoid() && !other.bounds.IsVoid() && bounds.Distance(other.bounds)>clearance+checks.distance)
      return false;
    return separation(shape,other.shape)<=clearance;
  }
};
inline Generated generate(Project& project,const std::string& assembly) {
  project.assembly(assembly);
  std::map<std::string,TopoDS_Shape> boards;
  for(const auto& id:project.boards()) if(project.body(id).assembly==assembly) boards.emplace(id,boxShape(project.body(id).box));
  const auto& data=project.data();
  // Named cutters use the configured blank envelope. Clearance is per side,
  // independently on X/Y/Z; zero reproduces the previous exact-fit dados.
  if(data.HasMember("cuts")) {
    array(data["cuts"],"cuts");
    for(const auto& cut:data["cuts"].GetArray()) {
      keys(cut,{"type","tool","targets","clearance"},"cut");
      auto type=str(required(cut,"type","cut"),"cut.type");
      if(type!="dado" && type!="box_cut") throw std::runtime_error("Unknown cut type: "+type);
      auto tool=str(required(cut,"tool","cut"),"cut.tool");
      Box cutter=project.body(tool).box;
      if(cut.HasMember("clearance")) {
        auto clearance=project.vector(cut["clearance"],"cut.clearance");
        for(int axis=0;axis<3;++axis) {
          if(clearance[axis]<0 || clearance[axis]>checks.maxDimension) throw std::runtime_error("Cut clearance must be nonnegative and within dimension limits");
          cutter.low[axis]-=clearance[axis]; cutter.size[axis]+=2*clearance[axis];
        }
      }
      auto shape=boxShape(cutter);
      const auto& targets=required(cut,"targets","cut"); array(targets,"cut.targets");
      if(targets.Empty()) throw std::runtime_error("Cut requires at least one target");
      std::set<std::string> seen;
      for(const auto& target:targets.GetArray()) {
        auto id=str(target,"cut target"); const auto& body=project.body(id);
        if(!body.board || id==tool || !seen.insert(id).second) throw std::runtime_error("Cut target must be a distinct board: "+id);
        if(body.assembly!=assembly) continue;
        auto& board=boards.at(id);
        double before=volume(board);
        board=difference(board,shape);
        if(before-volume(board)<=checks.volume) throw std::runtime_error("Cut does not remove material from "+id);
        if(solidVolumes(board).size()!=1) throw std::runtime_error("Cut must leave one valid solid: "+id);
      }
    }
  }
  std::map<std::string,ScrewProfile> profiles;
  if(data.HasMember("hardware_profiles")) {
    object(data["hardware_profiles"],"hardware_profiles");
    for(auto it=data["hardware_profiles"].MemberBegin();it!=data["hardware_profiles"].MemberEnd();++it) {
      auto name=str(it->name,"hardware profile"); profiles.emplace(name,profile(project,it->value,name));
    }
  }
  // One board may be machined by several hardware groups. Keep its pockets
  // together so spacing rules cannot be bypassed by splitting the groups.
  std::map<std::string,std::vector<Pocket>> pocketsByBoard;
  Generated screws;
  if(data.HasMember("hardware")) {
    array(data["hardware"],"hardware");
    for(const auto& group:data["hardware"].GetArray()) {
      keys(group,{"type","assembly","profile","pocketed","receiver","obstacles","count","origin","step","axis","name","start_number"},"hardware group");
      auto string=[&](const char* key){return str(required(group,key,"hardware group"),key);};
      if(string("type")!="pocket_screws") throw std::runtime_error("Unknown hardware operation: "+string("type"));
      auto groupAssembly=string("assembly"); project.assembly(groupAssembly);
      auto p=profiles.find(string("profile"));
      if(p==profiles.end()) throw std::runtime_error("Unknown hardware profile: "+string("profile"));
      auto pocketed=string("pocketed"),receiver=string("receiver");
      const auto& end=project.body(pocketed); const auto& side=project.body(receiver);
      if(!end.board || !side.board || pocketed==receiver || end.assembly!=groupAssembly || side.assembly!=groupAssembly) throw std::runtime_error("Hardware boards must be distinct boards in its assembly");
      int count=Project::integer(project.eval(required(group,"count","hardware")),"hardware.count");
      int first=Project::integer(project.eval(required(group,"start_number","hardware")),"hardware.start_number",1,1000000-count);
      auto name=string("name");
      auto origin=project.vector(required(group,"origin","hardware"),"hardware.origin");
      auto step=project.vector(required(group,"step","hardware"),"hardware.step");
      auto direction=project.vector(required(group,"axis","hardware"),"hardware.axis");
      double norm=std::hypot(direction[0],direction[1]);
      // This primitive models horizontal pocket screws. Rotation of complete
      // boards and arbitrary 3D drilling require a separate operation/schema.
      if(!std::isfinite(norm) || norm<checks.distance || direction[2]!=0) throw std::runtime_error("Pocket screw axis must be nonzero and horizontal (Z=0)");
      gp_Dir axis(direction[0]/norm,direction[1]/norm,0);
      std::vector<std::string> obstacles;
      const auto& list=required(group,"obstacles","hardware"); array(list,"hardware.obstacles");
      for(const auto& item:list.GetArray()) { auto id=str(item,"obstacle"); project.body(id); obstacles.push_back(id); }
      if(groupAssembly!=assembly) continue;
      const auto& h=p->second; double radius=h.pocketDiameter/2;
      auto envelope=unite(boxShape(end.box),boxShape(side.box));
      auto& previousPockets=pocketsByBoard[pocketed];
      for(int i=0;i<count;++i) {
        std::array<double,3> point;
        for(int a=0;a<3;++a) {
          point[a]=origin[a]+i*step[a];
          if(!std::isfinite(point[a]) || std::abs(point[a])>checks.maxDimension) throw std::runtime_error("Hardware location exceeds dimension limits");
        }
        if(point[2]-radius<=end.box.low[2]+h.edgeClearance || point[2]+radius>=end.box.high(2)-h.edgeClearance)
          throw std::runtime_error(name+": pocket does not clear board rim");
        gp_Pnt joint(point[0],point[1],point[2]);
        auto seat=along(joint,axis,-h.seatDistance);
        auto head=BRepPrimAPI_MakeCylinder(gp_Ax2(along(seat,axis,-h.headHeight),axis),h.headDiameter/2,h.headHeight).Shape();
        auto shaft=BRepPrimAPI_MakeCylinder(gp_Ax2(seat,axis),h.diameter/2,h.length-h.tipLength).Shape();
        auto tip=BRepPrimAPI_MakeCone(gp_Ax2(along(seat,axis,h.length-h.tipLength),axis),h.diameter/2,0,h.tipLength).Shape();
        auto screw=unite(unite(head,shaft),tip);
        if(!BRepCheck_Analyzer(screw).IsValid() || volume(difference(screw,envelope))>checks.volume || overlap(screw,boxShape(side.box))<=checks.volume)
          throw std::runtime_error(name+": screw leaves its joint envelope or misses receiver");
        Bnd_Box bounds; BRepBndLib::AddOptimal(screw,bounds,false,false);
        double b[6]; bounds.Get(b[0],b[1],b[2],b[3],b[4],b[5]);
        int into=std::abs(axis.X())>=std::abs(axis.Y())?0:1;
        double component=into==0?axis.X():axis.Y();
        double tipGap=component<0?b[into]-side.box.low[into]:side.box.high(into)-b[into+3];
        if(tipGap<=h.tipClearance) throw std::runtime_error(name+": screw does not clear receiver outside face");
        double boreLength=std::hypot(end.box.size[0],end.box.size[1])+h.seatDistance+h.headHeight;
        auto pocket=BRepPrimAPI_MakeCylinder(gp_Ax2(along(seat,axis,-boreLength),axis),radius,boreLength).Shape();
        auto pilot=BRepPrimAPI_MakeCylinder(gp_Ax2(seat,axis),h.diameter/2,h.length).Shape();
        for(const auto& id:obstacles) {
          auto obstacle=boxShape(project.body(id).box);
          if(overlap(screw,obstacle)>checks.volume || overlap(pocket,obstacle)>checks.volume || separation(pocket,obstacle)<h.edgeClearance-checks.distance)
            throw std::runtime_error(name+": screw/pocket violates clearance to "+id);
        }
        // The cutter extends outside the stock so it opens the board face.
        // Compare only the actual pocket inside this board; otherwise cutters
        // from opposite joints appear to touch far outside the material.
        Pocket machinedPocket(intersection(pocket,boxShape(end.box)),h.edgeClearance);
        for(const auto& other:previousPockets)
          if(machinedPocket.tooCloseTo(other))
            throw std::runtime_error(name+": adjacent pockets are too close");
        previousPockets.push_back(std::move(machinedPocket));
        auto& endShape=boards.at(pocketed); double before=volume(endShape);
        endShape=difference(difference(endShape,pocket),pilot);
        if(before-volume(endShape)<=checks.volume) throw std::runtime_error(name+": pocket misses its board");
        boards.at(receiver)=difference(boards.at(receiver),screw);
        screws.add(screw,numbered("S",first+i,name+" "+std::to_string(i+1)),part_role::Role::Hardware);
      }
    }
  }
  Generated result;
  for(const auto& id:project.boards()) if(project.body(id).assembly==assembly) {
    result.add(boards.at(id),numbered("B",project.body(id).number,project.body(id).name),part_role::Role::Board);
  }
  result.append(screws);
  if(std::set<std::string>(result.names.begin(),result.names.end()).size()!=result.names.size()) throw std::runtime_error("Duplicate part names in "+assembly);
  for(std::size_t i=0;i<result.parts.size();++i) {
    if(solidVolumes(result.parts[i]).size()!=1) throw std::runtime_error(result.names[i]+": expected one valid solid");
    for(std::size_t j=0;j<i;++j) if(overlap(result.parts[i],result.parts[j])>checks.volume)
      throw std::runtime_error("Configured parts overlap: "+result.names[i]+" and "+result.names[j]);
  }
  const auto& definition=data["assemblies"][assembly.c_str()];
  if(definition.HasMember("part_order")) {
    const auto& order=definition["part_order"]; array(order,"assembly.part_order");
    Generated ordered;
    std::set<std::string> seen;
    for(const auto& entry:order.GetArray()) {
      auto id=str(entry,"assembly.part_order");
      if(!seen.insert(id).second) throw std::runtime_error("Duplicate part_order entry: "+id);
      if(id=="@hardware") {
        ordered.append(screws);
      } else {
        const auto& body=project.body(id);
        if(!boards.count(id)) throw std::runtime_error("part_order must name boards in its assembly: "+id);
        ordered.add(boards.at(id),numbered("B",body.number,body.name),part_role::Role::Board);
      }
    }
    if(ordered.parts.size()!=result.parts.size()) throw std::runtime_error("part_order must include every board and @hardware when present");
    result=std::move(ordered);
  }
  return result;
}
inline void exportProject(Project& project,const std::string& assembly,const std::filesystem::path& output) {
  if(output.empty()) throw std::runtime_error("Output path cannot be empty");
  auto generated=generate(project,assembly);
  auto expected=solidVolumes(compound(generated.parts));
  if(output.has_parent_path()) std::filesystem::create_directories(output.parent_path());
  TemporaryStep temporary(output,"writing");
  auto imported=writeNamedAssembly(temporary.file(),generated.parts,generated.names,generated.roles,project.assembly(assembly).name);
  verifySolidVolumes(imported,expected,generated.parts.size());
  temporary.publish(output);
  std::cout<<"Saved and verified project assembly '"<<assembly<<"': "<<std::filesystem::absolute(output)<<"\n";
}
}

#define main merge_main
#include "step_merge.cpp"
#undef main
int main(int argc,char** argv) {
  try {
    if(argc!=3) throw std::runtime_error("Expected normal and rotated files");
    auto normal=snapshot(read(argv[1])), rotated=snapshot(read(argv[2]));
    if(normal.size()!=rotated.size()) throw std::runtime_error("Part count changed");
    for(std::size_t i=0;i<normal.size();++i) {
      auto a=normal[i],b=rotated[i];
      if(a.path!=b.path || a.solids!=b.solids) throw std::runtime_error("Names/hierarchy changed");
      const auto& x=a.bounds;
      const std::array<double,6> expected{x[0],x[2],-x[4],x[3],x[5],-x[1]};
      for(int k=0;k<6;++k) if(std::abs(expected[k]-b.bounds[k])>1e-5) throw std::runtime_error("Incorrect axis mapping");
      if(std::abs(a.volume-b.volume)>std::max(1e-5,std::abs(a.volume)*1e-7)) throw std::runtime_error("Volume changed");
    }
    std::cout<<"Verified X,Y,Z -> X,Z,-Y for every part; names and hierarchy preserved.\n";
  } catch(const std::exception&e) {std::cerr<<e.what()<<'\n';return 1;}
}

#pragma once
#include "part_role.hpp"
#include <XCAFDoc_LayerTool.hxx>

namespace part_role {
inline constexpr int boardFlag=1;
inline constexpr int hardwareFlag=2;

inline int flags(const Handle(XCAFDoc_LayerTool)& layers,
                 const TDF_Label& instance,const TDF_Label& definition) {
  int result=0;
  if(layers->IsSet(definition,TCollection_ExtendedString(boardLayer))
     || layers->IsSet(instance,TCollection_ExtendedString(boardLayer))) result|=boardFlag;
  if(layers->IsSet(definition,TCollection_ExtendedString(hardwareLayer))
     || layers->IsSet(instance,TCollection_ExtendedString(hardwareLayer))) result|=hardwareFlag;
  return result;
}
}

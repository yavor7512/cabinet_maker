#pragma once
#ifdef __APPLE__
#include <CoreText/CoreText.h>
#endif
namespace packet {
struct FontData {std::string bytes,widths;double ascent=950,descent=-250;};
inline FontData helveticaNeue(){
#ifdef __APPLE__
 CTFontRef font=CTFontCreateWithName(CFSTR("HelveticaNeue"),1000,nullptr);
 if(!font)throw std::runtime_error("Helvetica Neue is not installed");
 FontData result;result.ascent=CTFontGetAscent(font);result.descent=-CTFontGetDescent(font);
 std::ostringstream widths;for(int i=32;i<=126;++i){UniChar ch=i;CGGlyph glyph;CTFontGetGlyphsForCharacters(font,&ch,&glyph,1);CGSize advance;CTFontGetAdvancesForGlyphs(font,kCTFontOrientationHorizontal,&glyph,&advance,1);widths<<advance.width<<' ';}result.widths=widths.str();
 CFArrayRef tags=CTFontCopyAvailableTables(font,kCTFontTableOptionNoOptions);
 struct Table{uint32_t tag;std::string data;};std::vector<Table> tables;
 for(CFIndex i=0;i<CFArrayGetCount(tags);++i){auto tag=static_cast<CTFontTableTag>(reinterpret_cast<uintptr_t>(CFArrayGetValueAtIndex(tags,i)));if(tag==0x44534947)continue;CFDataRef data=CTFontCopyTable(font,tag,kCTFontTableOptionNoOptions);if(data){tables.push_back({uint32_t(tag),std::string(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),CFDataGetLength(data))});CFRelease(data);}}
 CFRelease(tags);CFRelease(font);
 std::sort(tables.begin(),tables.end(),[](auto&a,auto&b){return a.tag<b.tag;});
 auto put16=[](std::string&s,size_t at,uint16_t n){s[at]=char(n>>8);s[at+1]=char(n);};
 auto put32=[](std::string&s,size_t at,uint32_t n){for(int j=0;j<4;++j)s[at+j]=char(n>>(24-8*j));};
 auto sum=[](const std::string&s){uint32_t v=0;for(size_t i=0;i<s.size();i+=4){uint32_t w=0;for(int j=0;j<4;++j)w=(w<<8)|(i+j<s.size()?static_cast<unsigned char>(s[i+j]):0);v+=w;}return v;};
 auto&b=result.bytes;b.resize(12+16*tables.size(),0);put32(b,0,0x10000);put16(b,4,tables.size());int power=1,selector=0;while(power*2<=int(tables.size())){power*=2;++selector;}put16(b,6,power*16);put16(b,8,selector);put16(b,10,tables.size()*16-power*16);
 size_t head=0;bool glyf=false;
 for(size_t i=0;i<tables.size();++i){auto&t=tables[i];if(t.tag==0x676c7966)glyf=true;if(t.tag==0x68656164&&t.data.size()>=12){put32(t.data,8,0);head=b.size();}auto offset=b.size();put32(b,12+16*i,t.tag);put32(b,16+16*i,sum(t.data));put32(b,20+16*i,offset);put32(b,24+16*i,t.data.size());b+=t.data;while(b.size()%4)b+='\0';}
 if(!glyf||!head)throw std::runtime_error("Helvetica Neue TrueType outlines unavailable");
 put32(b,head+8,0xB1B0AFBA-sum(b));return result;
#else
 throw std::runtime_error("Embedded Helvetica Neue currently requires macOS CoreText");
#endif
}
}

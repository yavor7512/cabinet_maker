#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace design {
// Deliberately small arithmetic language. No scripting, files, shell or eval.
class Expression {
  const std::string& text;
  const std::function<double(const std::string&)> lookup;
  std::size_t pos = 0;
  int depth = 0;
  void spaces() { while(pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos; }
  bool take(char c) { spaces(); if(pos < text.size() && text[pos] == c) { ++pos; return true; } return false; }
  [[noreturn]] void fail(const std::string& reason) const {
    throw std::runtime_error(reason + " at column " + std::to_string(pos+1) + " in '" + text + "'");
  }
  double atom() {
    if(++depth > 128) fail("Expression nesting is too deep");
    double result;
    if(take('+')) result = atom();
    else if(take('-')) result = -atom();
    else if(take('(')) { result = sum(); if(!take(')')) fail("Expected ')'"); }
    else {
      spaces();
      if(pos == text.size()) fail("Expected a number or reference");
      if(std::isdigit(static_cast<unsigned char>(text[pos])) || text[pos] == '.') {
        std::size_t used = 0;
        try { result = std::stod(text.substr(pos), &used); }
        catch(const std::exception&) { fail("Invalid number"); }
        pos += used;
      } else {
        auto start = pos;
        while(pos < text.size() && (std::isalnum(static_cast<unsigned char>(text[pos])) || text[pos]=='_' || text[pos]=='.')) ++pos;
        if(pos == start) fail("Expected a number or reference");
        auto name = text.substr(start, pos-start);
        if(take('(')) {
          std::vector<double> args;
          if(!take(')')) { do { args.push_back(sum()); } while(take(',')); if(!take(')')) fail("Expected ')'"); }
          if(name=="min" && args.size()==2) result = std::min(args[0],args[1]);
          else if(name=="max" && args.size()==2) result = std::max(args[0],args[1]);
          else if(name=="abs" && args.size()==1) result = std::abs(args[0]);
          else if(name=="sin_deg" && args.size()==1) result = std::sin(args[0]*std::acos(-1.0)/180);
          else if(name=="cos_deg" && args.size()==1) result = std::cos(args[0]*std::acos(-1.0)/180);
          else fail("Unknown function or incorrect argument count: " + name);
        } else result = lookup(name);
      }
    }
    --depth;
    if(!std::isfinite(result)) fail("Non-finite result");
    return result;
  }
  double product() {
    double v=atom();
    while(true) {
      if(take('*')) v*=atom();
      else if(take('/')) { double divisor=atom(); if(divisor==0) fail("Division by zero"); v/=divisor; }
      else return v;
    }
  }
  double sum() { double v=product(); while(true) { if(take('+')) v+=product(); else if(take('-')) v-=product(); else return v; } }
public:
  Expression(const std::string& input, std::function<double(const std::string&)> resolve): text(input), lookup(std::move(resolve)) {}
  double run() {
    if(text.empty() || text.size()>8192) fail("Expression must contain 1..8192 characters");
    double v=sum(); spaces();
    if(pos!=text.size()) fail("Unexpected token");
    if(!std::isfinite(v)) fail("Non-finite result");
    return v;
  }
};

class Graph {
  struct Entry { std::string expression; int state=0; double result=0; };
  std::map<std::string,Entry> entries;
  std::vector<std::string> stack;
public:
  void add(const std::string& name, const std::string& expression) {
    if(!entries.emplace(name, Entry{expression}).second) throw std::runtime_error("Duplicate reference: " + name);
  }
  double evaluate(const std::string& expression) {
    return Expression(expression, [&](const std::string& name) { return value(name); }).run();
  }
  double value(const std::string& name) {
    auto it=entries.find(name);
    if(it==entries.end()) throw std::runtime_error("Unknown reference: " + name);
    auto& e=it->second;
    if(e.state==2) return e.result;
    if(e.state==1) {
      std::string chain;
      for(const auto& ref:stack) chain+=ref+" -> ";
      throw std::runtime_error("Circular relationship: " + chain + name);
    }
    if(stack.size()>=256) throw std::runtime_error("Relationship chain exceeds 256 references at " + name);
    stack.push_back(name); e.state=1;
    try { e.result=evaluate(e.expression); e.state=2; stack.pop_back(); return e.result; }
    catch(const std::exception& error) {
      e.state=0; stack.pop_back();
      throw std::runtime_error(name+": "+error.what());
    }
  }
  void validate() { for(const auto& pair:entries) value(pair.first); }
};
}

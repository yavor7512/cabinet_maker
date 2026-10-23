#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

// Keep a STEP export private until it has been reimported and verified. The
// directory reservation is atomic, so independent exporters cannot share a
// temporary file. Existing stale slots belong to earlier runs and stay intact.
class TemporaryStep {
  std::filesystem::path directory_;
  std::filesystem::path file_;
public:
  TemporaryStep(const std::filesystem::path& output,const char* operation) {
    for(int slot=0;slot<1000;++slot) {
      auto candidate=output;
      candidate += std::string(".")+operation+"."+std::to_string(slot);
      std::error_code error;
      if(std::filesystem::create_directory(candidate,error)) {
        directory_=candidate;
        file_=directory_/output.filename();
        return;
      }
      if(error && error!=std::errc::file_exists)
        throw std::filesystem::filesystem_error("Could not reserve temporary STEP directory",candidate,error);
    }
    throw std::runtime_error("No temporary STEP slot available for "+output.string());
  }
  TemporaryStep(const TemporaryStep&)=delete;
  TemporaryStep& operator=(const TemporaryStep&)=delete;
  ~TemporaryStep() {
    std::error_code ignored;
    if(!directory_.empty()) std::filesystem::remove_all(directory_,ignored);
  }
  const std::filesystem::path& file() const {return file_;}
  void publish(const std::filesystem::path& output) {
    std::filesystem::rename(file_,output);
  }
};

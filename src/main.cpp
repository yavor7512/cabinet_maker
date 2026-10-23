#include "project_cli.hpp"

int main(int argc, char** argv) {
  try {
    return design::projectCommand(argc, argv);
  } catch (const Standard_Failure& error) {
    std::cerr << "OpenCascade error: " << error.GetMessageString() << '\n';
  } catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
  }
  return 1;
}

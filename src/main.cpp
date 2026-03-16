#include "app.hpp"

int main(){
  try {
    App app;
    app.run();
  }
  catch (const std::exception& e) {
    std::cerr << "Fatal Error: "<< e.what() << '\n';
    return -1;
  }
  return 0;
}
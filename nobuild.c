#define NOBUILD_IMPLEMENTATION
#include "./nobuild.h"

#define CC "g++"
#define CXXFLAGS "-std=c++11", "-Wall", "-Wextra", "-pedantic", "-O3", "-g"
#define LIBS "-lX11"
#define EXE "./build/life"


int main (int argc, char **argv) {
  GO_REBUILD_URSELF(argc, argv);

  if (argc > 1) {
    CMD(CC, CXXFLAGS, "-o", EXE, "src/main.cpp", LIBS);
    
    if (strcmp(argv[1], "run") == 0) {
      CMD(EXE);
    } else if (strcmp(argv[1], "gdb") == 0) {
      CMD("st", "-e", "gdb", "--tui", EXE);
    } else if (strcmp(argv[1], "term") == 0) {
      CMD("st", "-e", EXE);
    } else if (strcmp(argv[1], "phone") == 0) {
      CMD(EXE, "-p", "sparse.ppm", "1440", "3120", "1500", "0.04");
      CMD("convert", "sparse.ppm", "sparse.png");
      CMD("rm", "sparse.ppm");

      CMD(EXE, "-p", "dense.ppm", "1440", "3120", "1000", "0.085");
      CMD("convert", "dense.ppm", "dense.png");
      CMD("rm", "dense.ppm");
    } else {
      CMD(CC, CXXFLAGS, "-o", EXE, "src/main.cpp", LIBS);
    }
  }
  return 0;
}

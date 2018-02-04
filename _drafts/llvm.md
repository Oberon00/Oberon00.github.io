1. Build LLVM with Clang
  - Dowload LLVM and Clang sources (3.8)
  - Copy the root direcory of the clang (cfe) tar to LLVM's tools direcory
    with the name "clang"
  - Create a new direcory anwhere e.g. b-llvm as a sibling of the llvm root
  - cd into that directory
  - Execute cmake <llvm-root-directory>
  - Execute "make -jn", replacing n with the number of CPU cores you have.
    This will take quite some time (~2 hours).

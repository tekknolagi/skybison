{
  description = "A flake for building Hello World";
  inputs.nixpkgs.url = github:NixOS/nixpkgs/a46c5b471eea21bb4d1b75c9f3d3d8e66f2c3a1b;
  outputs = { self, nixpkgs }: {
    defaultPackage.x86_64-linux =
      # Notice the reference to nixpkgs here.
      with import nixpkgs { system = "x86_64-linux"; };
      stdenv.mkDerivation {
        pname = "python";
        name = "skybison";
        src = self;
        dontUseCmakeConfigure=true;
        buildInputs = [
        cmake
        clang
        ninja
        python38

        bzip2
        libffi
        lzma
        openssl
        readline
        sqlite
        zlib
        ];
        buildPhase = ''
        cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=util/linux.cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
        ninja -C build python
        '';
        installPhase = "mv build $out";
      };
  };
}

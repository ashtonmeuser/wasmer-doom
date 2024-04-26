all: wasmer.out wasmtime.out

wasmer.out: src/*
	clang++ -std=c++11 -Iwasmer/include -Lwasmer/lib -o wasmer.out -arch x86_64 -arch arm64 -framework Cocoa -Wl,-undefined,dynamic_lookup src/main.cpp ./wasmer/lib/libwasmer.a

wasmtime.out: src/*
	clang++ -std=c++11 -Iwasmtime/include -Lwasmtime/lib -o wasmtime.out -arch x86_64 -arch arm64 -framework Cocoa -Wl,-undefined,dynamic_lookup src/main.cpp ./wasmtime/lib/libwasmtime.a

universal: wasmer/*
	lipo wasmer/lib/libwasmer.*64.a -output wasmer/lib/libwasmer.a -create
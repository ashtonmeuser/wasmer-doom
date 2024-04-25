main: src/*
	clang++ -std=c++11 -Iwasmer/include -Lwasmer/lib -o main -arch x86_64 -arch arm64 -framework Cocoa -Wl,-undefined,dynamic_lookup src/main.cpp ./wasmer/lib/libwasmer.a

universal: wasmer/*
	lipo wasmer/lib/libwasmer.*64.a -output wasmer/lib/libwasmer.a -create
# DLL Dependencies Parser

A command line application for finding recursive `DLL` dependencies on an `EXE` or `DLL` as well as a way of listing which `DLL`s cannot be loaded or found.

## Why?

When you launch an `EXE` or load a `DLL`, it might fail due to some missing `DLL`. [`Windows` generally does not make debugging `DLL` loading failures easy](https://en.wikipedia.org/wiki/DLL_Hell). [You might only receive an application error code indicating `DLL` load failure](https://github.community/t/status-dll-not-found-error-exit-code-0xc0000135/17115) and no detailed report as to why and which `DLL` was actually missing. There are ways of listing `DLL` dependencies such as outlined [here](https://stackoverflow.com/questions/7378959) but the `DLL Dependencies Parser` can be implemented into any application easily via an external process call and it can accurately report `DLL` load failures. Furthermore, the application is fully statically compiled (all libraries and `CRT`) to only depend on `KERNEL32.dll` to ironically not fall victim to the problem it tries to solve:

```
>dumpbin /dependents DLL-Dependencies-Parser.exe
Microsoft (R) COFF/PE Dumper Version 14.28.29336.0
Copyright (C) Microsoft Corporation.  All rights reserved.


Dump of file DLL-Dependencies-Parser.exe

File Type: EXECUTABLE IMAGE

  Image has the following dependencies:

    KERNEL32.dll

  Summary

        B000 .data
        8000 .pdata
       31000 .rdata
        2000 .reloc
        1000 .rsrc
       B2000 .text
        1000 _RDATA
```

Note that `dumpbin` is only available if you installed `Visual Studio`'s `C++` development tools so it is not suitable for distribution or reasonable to assume it is even available on the user's system.

## Usage

```
DLL-Dependencies-Parser.exe [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --pe-file-path TEXT:FILE REQUIRED
                              The file path to the executable to analyze
  --skip-parsing-windows-dll-dependencies
                              Whether Windows DLLs will not be parsed to speed up analysis
  --results-output-file-path TEXT
                              The output file to write the results to
```

### Example:

```batch
>DLL-Dependencies-Parser.exe --pe-file-path D:\My-Application.exe --results-output-file-path D:\Results.json --skip-parsing-windows-dll-dependencies
```

Now the `DLL` loading report of `D:\My-Application.exe` is written to the `D:\Results.json` file and can be examined manually or programmatically.

### Potential Errors

`Failed parsing PE file`: This error means that the input file wasn't a valid PE file. This error is returned by the `pe-parse` library.

## Compiling

This project is a `Visual Studio` solution and therefore the `*.sln` file has to be loaded in `Visual Studio`. You also need a `C++20` compatible `MSVC` compiler. Furthermore, this application depends on various libraries which need to be available on the system via a package manager like [`vcpkg`](https://github.com/microsoft/vcpkg) by running the `vcpkg install [package-name]` command:

* [`CLI11`](https://github.com/CLIUtils/CLI11)
* [`spdlog`](https://github.com/gabime/spdlog)
* [`pe-parse`](https://github.com/trailofbits/pe-parse)
* [`nlohmann-json`](https://github.com/nlohmann/json)
* [`Boost`](https://www.boost.org)

Furthermore, don't forget to `vcpkg integrate install` with `Visual Studio`.

### Tests

The binaries required to run the tests successfully are currently **not** provided.

## Credits

The open source libraries above which greatly simplified the development of this tool and `BullyWiiPlaza` for the design and implementation.
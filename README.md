# easy-phi

<h4 align="center">

![easy-phi](https://socialify.git.ci/qaqFei/easy-phi/image?description=1&descriptionEditable=A%20simple,%20high-performance,%20and%20cross-platform%20library%20for%20rendering%20Phigros%20charts.&font=Jost&forks=1&issues=1&name=1&owner=1&pattern=Charlie%20Brown&pulls=1&stargazers=1&theme=Auto)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Language](https://img.shields.io/badge/language-C++-blue.svg)
</h4>

easy-phi is a library written in C++ to rendering phigros chart with no dependencies and all the platforms are supported.

## Features

- **Chart Loading**: Supports loading official Phigros and charts created by Re:PhiEdit.
- **High Performance**: Optimized for performance, suitable for real-time rendering.
- **Simple API**: Easy to use and understand.
- **Cross Platform**: Works on any platform that supports C++.
- **Zero Dependencies**: No external libraries are required.

## Examples

For the completed example, check out [here](./test_files/test.cpp).

And the `Open RPE Recorder` is a subproject of this project, you can find it [here](./test_files/open_rpe_recorder.cpp), we also provide its [release](https://github.com/qaqFei/easy-phi/releases).

## Building

**Note**: This library requires **C++20 or later**.

### Library

Just include the [`easy_phi.hpp`](./src/easy_phi.hpp) header file in your project and you're ready to go!

### Tests

Tests only supports Windows, and you need to install [python](https://www.python.org/downloads/) and  [mingw-w64](https://mingw-w64.org/doku.php/download) that supports C++20 or later to build the test programs.

You can run the main test program by the following command:

```bash
python ./build_test.py --run --debug
```

## License

This project is licensed under the [MIT License](./LICENSE).

## Other

The charts in `test_files` are **not owned** by this project, please contact for removal if there is any copyright infringement.

**We do not guarantee that the api is stable between versions.**

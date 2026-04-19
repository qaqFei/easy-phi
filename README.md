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

Refer to the [documentation](./docs) for more details.

## Examples

### Loading a Chart

```cpp
#include <easy_phi.hpp>
using namespace easy_phi;

#include <iostream>

int main() {
    Data chartData;
    Data::FromFile(&chartData, "./path/to/chart.json");

    auto loadResult = loadChartFromData(chartData);

    if (loadResult.success) {
        auto& chart = loadResult.chart;
        chart.init();

        // Do something with the chart
    } else {
        std::cerr << "Failed to load chart:" << std::endl;

        for (const auto& e : loadResult.errors) {
            std::cerr << e << std::endl;
        }

        return 1;
    }

    return 0;
}
```

### Calculating a Frame to Render

```cpp
#include <easy_phi.hpp>
using namespace easy_phi;

#include <iostream>

int main() {
    // load chart
    Data chartData;
    Data::FromFile(&chartData, "./path/to/chart.json");

    auto chart = loadChartFromData(chartData).chart;
    chart.init();

    // configure
    CalculateFrameConfig config {
        .screenSize = { 1920.0, 1080.0 },
        .backgroundTextureSize = { 1920.0, 1440.0 },
        .songLength = 150.0
    };

    config.noteTextureInfos[EnumPhiNoteType::Tap] = CalculateFrameConfig::NoteTextureInfo {
        .single = CalculateFrameConfig::NoteTextureInfo::Item {
            .textureSize = Vec2 { 128.0, 32.0 },
            .cutPadding = Vec2 { 64.0, 64.0 }
        },
        .simul = CalculateFrameConfig::NoteTextureInfo::Item {
            .textureSize = Vec2 { 128.0, 32.0 },
            .cutPadding = Vec2 { 64.0, 64.0 }
        }
    }; // and configure other note types...

    // calculate frame
    double time = 1.25;
    CalculatedFrame frame {};
    calculateFrame(chart, time, config, frame);

    // render...

    return 0;
}
```

For the completed example, check out [here](./test_files/test.cpp).

And the `Open RPE Recorder` is a subproject of this project, you can find it [here](./test_files/open_rpe_recorder.cpp), we also provide its [release](https://github.com/qaqFei/easy-phi/releases).

## Building

Just include the [`easy_phi.hpp`](./src/easy_phi.hpp) header file in your project and you're ready to go!

**Note**: This library requires **C++20 or later**.

## License

This project is licensed under the [MIT License](./LICENSE).

## Other

The charts in `test_files` are **not owned** by this project, please contact for removal if there is any copyright infringement.

**We do not guarantee that the api is stable between versions.**

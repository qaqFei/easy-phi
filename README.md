# easy-phi

<h4 align="center">

![easy-phi](https://socialify.git.ci/qaqFei/easy-phi/image?description=1&descriptionEditable=A%20simple%20and%20high-performance%20library%20for%20rendering%20rhythm%20charts.&font=Jost&forks=1&issues=1&name=1&owner=1&pattern=Charlie%20Brown&pulls=1&stargazers=1&theme=Auto)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Language](https://img.shields.io/badge/language-C++-blue.svg)
</h4>

easy-phi is a library written in C++ to rendering rhythm chart.

It is based on [grain](https://github.com/qaqFei/grain), a C++ package manager.

## Games Supported

- Phigros
- Milthm

## Features

- **High Performance**: Optimized for performance, suitable for real-time rendering.
- **Simple API**: Easy to use and understand.
- **Game Utilities**: Support for handling game-related data or resources.

## Working On

- **Making it Easier**: We're making it easier to use easy-phi by clear and convient API design.

- **Supporting More Games**: We're working on supporting more rhythm games, such as [Rizline](https://store.steampowered.com/app/2272590/Rizline/).

## Usage

You should install the grain package manager:

```bash
pip install git+https://github.com/qaqFei/grain.git
grain info # init config

# if you are in China, you can use ghproxy
grain config set use_ghproxy True
```

### Use it in your project

See the documentation [here](https://github.com/qaqFei/grain/blob/main/docs/using_in_cmake.md).

### Tests

You can run the main test program by the following command:

```bash
grain package clean
grain package run-test . --macro APP_TYPE_OPEN_RPE_RECORDER
```

## Other

The files in `files/resources` are **maybe not owned** by this project, please contact for removal if there is any copyright infringement.

**We do not guarantee that the api is stable between versions.**

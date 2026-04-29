# Contributing to romHEX 14

Thank you for your interest in contributing to romHEX 14! Whether you're fixing a bug, adding a feature, improving documentation, or reporting an issue — your contribution is valued.

## Getting Started

1. **Fork** the repository on [GitHub](https://github.com/ctabuyo/romHEX14-community) or [Gitee](https://gitee.com/ctabuyo/romHEX14-community)
2. **Clone** your fork locally
3. **Build** the project (see [README.md](README.md#building-from-source))
4. **Create a branch** for your work: `git checkout -b feature/my-feature`

## Development Setup

```bash
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt6 -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
```

The community edition builds with `RX14_PRO_BUILD=OFF` (the default).

## Code Style

| Convention | Example |
|---|---|
| Indentation | 4 spaces (no tabs) |
| Methods | `camelCase()` |
| Classes | `PascalCase` |
| Members | `m_camelCase` |
| Local variables | `camelCase` |
| Constants | `kPascalCase` or `ALL_CAPS` |
| Files | `lowercase.cpp` / `lowercase.h` |

### General Guidelines

- Use C++17 features where appropriate (structured bindings, `if constexpr`, `std::optional`, etc.)
- Follow Qt6 conventions for signal/slot connections (`connect()` with function pointers)
- Prefer `QStringLiteral()` over `QString("")` for string literals
- Every new `.cpp`/`.h` file must include the copyright header (see any existing file for the template)

## Submitting Changes

### Pull Requests

1. Keep PRs focused — one feature or fix per PR
2. Write a clear title and description explaining **what** and **why**
3. If your PR addresses an issue, reference it: `Fixes #123`
4. Make sure the project builds without errors before submitting

### Commit Messages

Use clear, imperative commit messages:

```
Add map pack export dialog

Implement the export dialog that lets users select which maps to
include in the .rxpack file. Supports filtering by group and
drag-and-drop reordering.
```

## Reporting Issues

- **GitHub Issues** — preferred for English speakers
- **Gitee Issues** — available for Chinese users who cannot access GitHub

When reporting a bug, please include:
- romHEX 14 version (from Help → About)
- Operating system and version
- Steps to reproduce
- Expected vs actual behavior
- Screenshots if applicable

## Architecture Overview

The application follows a standard Qt6 MDI (Multiple Document Interface) architecture:

- `MainWindow` — Top-level window, menu bar, toolbar, project tree
- `Project` — Represents one open ROM file with maps, metadata, and editing state
- `ProjectView` — MDI sub-window wrapping hex/waveform/3D views
- `MapOverlay` — Inline 2D map editor with axis display
- `src/io/ols/` — OLS/KP file format parser (import only in community edition)

## License

By contributing to romHEX 14, you agree that your contributions will be licensed under the [GNU General Public License v3.0](LICENSE).

# ofod11

A code companion for John C. Hull, *Options, Futures, and Other Derivatives*
(11th edition), in C++20 and Python 3, targeting Ubuntu Linux.

The repo holds three independent components:

| Component | Path | What it is |
| --- | --- | --- |
| C++ library | `cpp/` | `libofod`, C++20, no Python dependency |
| Python library | `python/` | `ofod`, pure Python (NumPy/SciPy) |
| Bindings | `bindings/` | `ofod_cpp`, nanobind wrapper around `libofod` |

The full plan is in [docs/plan.md](docs/plan.md). Every chapter (1-37) has
an example under `chapters/` in both languages, and the docs site turns
them into one page per chapter plus a Python API reference:

```sh
pip install -r docs/requirements.txt
mkdocs serve   # or `mkdocs build` to write site/
```

## Quick start (Windows + Docker Desktop)

The repo lives at `C:\develop\ofod11`; `C:\develop` is mounted into the
container at `/develop`. Build output and caches stay in Docker volumes.

```powershell
cd C:\develop
git clone https://github.com/dzheresc/ofod11.git
cd ofod11
docker compose -f docker/compose.yaml build
docker compose -f docker/compose.yaml run --rm dev ./tools/bootstrap.sh
```

`bootstrap.sh` builds and tests all three components: CMake preset
`gcc-debug` + CTest, then the Python library and the bindings with pytest.
Pass another preset to try it, e.g. `./tools/bootstrap.sh clang-asan`.

For an interactive shell: `docker compose -f docker/compose.yaml run --rm dev`.
In VS Code, "Dev Containers: Reopen in Container" uses the same setup.

## Code style

All code follows the Google style guides:

* C++: [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
  `.clang-format` (`BasedOnStyle: Google`) and `.clang-tidy` (`google-*` checks,
  Google naming: `CamelCase` functions and types, `snake_case` variables,
  `kConstant`, trailing `_` for private members, `.h`/`.cc` files,
  `#define` header guards).
* Python: [Google Python Style Guide](https://google.github.io/styleguide/pyguide.html).
  `ruff.toml` sets 80 columns, Google docstrings and Google import order.
* Bindings expose C++ `CamelCase` functions under Python `snake_case` names.

Run the formatters before committing:

```bash
clang-format -i $(git ls-files '*.h' '*.cc')
ruff format . && ruff check .
```

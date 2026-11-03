# OFOD 11 code companion

Code for John C. Hull's *Options, Futures and Other Derivatives*
(11th edition), in three separate components:

| Component | Where | What |
| --- | --- | --- |
| `libofod` | `cpp/` | C++20 library, Google style, tested with Catch2 and profiled with the Valgrind suite |
| `ofod` | `python/` | Pure-Python 3 package with the same API in snake_case |
| `ofod_cpp` | `bindings/` | nanobind module exposing `libofod` under the `ofod` names |

Each chapter has a page under **Chapters** with what the code covers,
how to run the C++ and Python examples, and both programs. Every chapter
example prints the same output from the C++ binary and from the Python
version run with `--cpp`. Values from the book are pinned in golden
tests under `data/golden/`, which all three components must reproduce.

The **Python API** pages are generated from the `ofod` docstrings. The C++
headers under `cpp/include/ofod/` carry the same documentation as
comments. The build plan and the profiling notes are under **Plan** and
**Performance**.

## Building the docs

```sh
pip install -r docs/requirements.txt
mkdocs build   # or: mkdocs serve
```

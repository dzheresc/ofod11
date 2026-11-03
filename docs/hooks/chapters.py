"""MkDocs hook: one page per chapter, the API reference and the nav.

Chapter pages are built from chapters/chNN_*/README.md at build time, with
both example programs included, so the READMEs stay the single source.
The generated pages land in docs/chapters and docs/api (both git-ignored).
"""

import pathlib
import re
from typing import Any

_ROOT = pathlib.Path(__file__).resolve().parents[2]
_DOCS = _ROOT / "docs"
_REPO = "https://github.com/dzheresc/ofod11/blob/main"
_API = (
    "core",
    "math",
    "instruments",
    "curves",
    "futures",
    "pricers",
    "greeks",
    "models",
    "random",
    "risk",
    "stats",
    "credit",
    "strategies",
)


def _write(path: pathlib.Path, text: str) -> None:
    # Only rewrite changed pages so `mkdocs serve` does not loop.
    if not path.exists() or path.read_text() != text:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)


def _sources(chapter: pathlib.Path, language: str, pattern: str) -> list[str]:
    return sorted(
        p.relative_to(_ROOT).as_posix()
        for p in (chapter / language).glob(pattern)
    )


def _chapter_page(chapter: pathlib.Path) -> tuple[str, str]:
    readme = (chapter / "README.md").read_text()
    title = readme.splitlines()[0].lstrip("# ").strip()
    match = re.match(r"ch(\d+)_", chapter.name)
    if match is None:
        raise ValueError(f"unexpected chapter directory {chapter.name}")
    number = int(match.group(1))
    lines = [readme.rstrip(), "", "## Running the examples", ""]
    lines += [
        "```sh",
        f"./build/gcc-release/chapters/{chapter.name}",
    ]
    for py in _sources(chapter, "py", "*.py"):
        lines += [f"python {py}", f"python {py} --cpp"]
    lines += ["```", ""]
    for heading, language, pattern, lang in (
        ("C++", "cpp", "*.cc", "cpp"),
        ("Python", "py", "*.py", "python"),
    ):
        for src in _sources(chapter, language, pattern):
            lines += [
                f'??? example "{heading}: [`{src}`]({_REPO}/{src})"',
                "",
                f"    ```{lang}",
                f'    --8<-- "{src}"',
                "    ```",
                "",
            ]
    lines += [
        f"*Hull, Options, Futures and Other Derivatives, 11th edition, "
        f"Chapter {number}.*",
        "",
    ]
    return title, "\n".join(lines)


def on_config(config: Any) -> Any:
    """Generates the pages and sets the nav before files are collected."""
    chapters = []
    for chapter in sorted(_ROOT.glob("chapters/ch[0-9][0-9]_*")):
        if not (chapter / "README.md").exists():
            continue
        title, text = _chapter_page(chapter)
        page = f"chapters/{chapter.name}.md"
        _write(_DOCS / page, text)
        chapters.append({title: page})
    api = []
    for name in _API:
        page = f"api/{name}.md"
        _write(
            _DOCS / page,
            f"# ofod.{name}\n\n::: ofod.{name}\n    options:\n"
            "      show_submodules: false\n",
        )
        api.append({f"ofod.{name}": page})
    config["nav"] = [
        {"Home": "index.md"},
        {"Chapters": chapters},
        {"Python API": api},
        {"Plan": "plan.md"},
        {"Performance": "PERF.md"},
        {"Library comparison": "comparison_plan.md"},
        {"Comparison results": "COMPARISON.md"},
    ]
    return config

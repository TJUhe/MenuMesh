#!/usr/bin/env python3

from pathlib import Path
import re
import sys


INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}
SCAN_ROOTS = ("include", "src", "apps", "examples")

# 依赖关系按模块列出，表示该模块可以使用的下层模块。
# 将依赖图集中维护，可让新增源模块成为明确的架构决策，
# 而不是在扫描器中再增加一个特殊分支。
MODULE_DEPENDENCIES = {
    "core": frozenset(),
    "common": frozenset({"core"}),
    "mesh_edit": frozenset({"common", "core"}),
    "io": frozenset({"core"}),
    "analysis": frozenset({"common", "core"}),
    "feature_detection": frozenset({"common", "core", "debugUtil"}),
    "simplification": frozenset(
        {"analysis", "common", "core", "feature_detection", "mesh_edit"}
    ),
    "api": frozenset({"analysis", "core", "feature_detection", "io", "simplification"}),
    "debugUtil": frozenset({"core"}),
}

INCLUDE_MODULE_PREFIXES = (
    ("algorithms/analysis/", "analysis"),
    ("algorithms/feature_detection/", "feature_detection"),
    ("algorithms/simplification/", "simplification"),
    ("analysis/", "analysis"),
    ("feature_detection/", "feature_detection"),
    ("simplification/", "simplification"),
    ("mesh_edit/", "mesh_edit"),
    ("debugUtil/", "debugUtil"),
    ("common/", "common"),
    ("core/", "core"),
    ("api/", "api"),
    ("io/", "io"),
)


def as_posix(path):
    return path.replace("\\", "/")


def include_is_private_path(include):
    inc = as_posix(include)
    return (
        inc.startswith("src/")
        or inc.startswith("../src/")
        or "/src/" in inc
        or inc.startswith("detail/")
        or "/detail/" in inc
        or inc.startswith("common/detail/")
        or inc.startswith("feature_detection/detail/")
        or inc.startswith("simplification/detail/")
        or inc.startswith("mesh_edit/detail/")
    )


def source_module(relative_path):
    parts = as_posix(relative_path).split("/")
    if len(parts) >= 3 and parts[0] == "src":
        return parts[1]
    return None


def include_module(include, current_module):
    inc = as_posix(include)
    if inc.startswith("detail/"):
        return current_module
    for prefix, module in INCLUDE_MODULE_PREFIXES:
        if inc.startswith(prefix):
            return module
    return None


def iter_source_files(root):
    for scan_root in SCAN_ROOTS:
        base = root / scan_root
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix in SOURCE_SUFFIXES:
                yield path


def iter_includes(path):
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except UnicodeDecodeError as exc:
        yield 0, "<encoding>", f"{path}: is not valid UTF-8 ({exc})"
        return

    for line_number, line in enumerate(lines, start=1):
        match = INCLUDE_RE.match(line)
        if match:
            yield line_number, match.group(1), None


def find_violations(root):
    violations = []

    for path in iter_source_files(root):
        rel = path.relative_to(root).as_posix()
        module = source_module(rel)
        if module is not None and module not in MODULE_DEPENDENCIES:
            violations.append(
                f"{rel}: source module '{module}' is missing from MODULE_DEPENDENCIES"
            )
            continue

        for line_number, include, error in iter_includes(path):
            if error:
                violations.append(error)
                continue

            public_or_app_surface = rel.startswith(("include/", "apps/", "examples/"))
            if public_or_app_surface and include_is_private_path(include):
                violations.append(
                    f"{rel}:{line_number}: public/app/example code must not include private path '{include}'"
                )

            dependency = include_module(include, module)
            if (
                module is not None
                and dependency is not None
                and dependency != module
                and dependency not in MODULE_DEPENDENCIES[module]
            ):
                violations.append(
                    f"{rel}:{line_number}: module '{module}' must not depend on "
                    f"'{dependency}' via '{include}'"
                )

    return violations


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    violations = find_violations(root)

    if violations:
        print("Include boundary violations:")
        for violation in violations:
            print(f"  - {violation}")
        return 1

    print("Include boundary check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3

from pathlib import Path
import tempfile

from check_include_boundaries import find_violations


def write_source(root, relative_path, contents):
    path = root / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8")


def check_case(relative_path, include, expected_fragment=None):
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        write_source(root, relative_path, f'#include "{include}"\n')
        violations = find_violations(root)
        if expected_fragment is None:
            assert not violations, violations
        else:
            assert len(violations) == 1, violations
            assert expected_fragment in violations[0], violations[0]


def main():
    check_case(
        "include/core/Public.h",
        "common/detail/MeshQueries.h",
        "public/app/example code must not include private path",
    )
    check_case(
        "src/common/Common.cpp",
        "algorithms/simplification/Metrics.h",
        "module 'common' must not depend on 'simplification'",
    )
    check_case(
        "src/feature_detection/Feature.cpp",
        "algorithms/simplification/SimplificationTypes.h",
        "module 'feature_detection' must not depend on 'simplification'",
    )
    check_case(
        "src/core/Mesh.cpp",
        "common/detail/MeshQueries.h",
        "module 'core' must not depend on 'common'",
    )
    check_case("src/mesh_edit/Edit.cpp", "common/detail/MeshQueries.h")
    check_case(
        "src/mesh_edit/Edit.cpp",
        "algorithms/simplification/SimplificationTypes.h",
        "module 'mesh_edit' must not depend on 'simplification'",
    )
    check_case(
        "src/simplification/Simplifier.cpp",
        "algorithms/feature_detection/FeatureTypes.h",
    )
    check_case("src/simplification/Simplifier.cpp", "mesh_edit/detail/MeshCompaction.h")
    check_case("src/common/Common.cpp", "core/Mesh.h")
    check_case("apps/manumesh/main.cpp", "core/Mesh.h")

    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        write_source(root, "src/repair/Repair.cpp", '#include "core/Mesh.h"\n')
        violations = find_violations(root)
        assert len(violations) == 1, violations
        assert "source module 'repair' is missing" in violations[0], violations[0]

    print("Include boundary checker tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

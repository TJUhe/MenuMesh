/*
 * Compile the public C ABI from C++ with hostile default calling-convention
 * and packing flags. Exact function-pointer types verify every declaration.
 */

#include <cstddef>
#include <type_traits>

#pragma pack(push, 2)
#include "api/CApi.h"

#pragma warning(push)
#pragma warning(disable : 4121)
struct ManuMeshCppPostHeaderPackProbe {
    char tag;
    double value;
};
#pragma warning(pop)
#pragma pack(pop)

static_assert(sizeof(ManuMeshVec2) == 16, "ManuMeshVec2 ABI size changed");
static_assert(sizeof(ManuMeshVec3) == 24, "ManuMeshVec3 ABI size changed");
static_assert(sizeof(ManuMeshFace) == 12, "ManuMeshFace ABI size changed");
static_assert(sizeof(ManuMeshFaceTexCoords) == 56, "ManuMeshFaceTexCoords ABI size changed");
static_assert(offsetof(ManuMeshFaceTexCoords, valid) == 48, "ManuMeshFaceTexCoords layout changed");
static_assert(sizeof(ManuMeshEdge) == 24, "ManuMeshEdge ABI size changed");
static_assert(offsetof(ManuMeshEdge, face_count) == 8, "ManuMeshEdge layout changed");
static_assert(sizeof(ManuMeshBounds) == 56, "ManuMeshBounds ABI size changed");
static_assert(offsetof(ManuMeshBounds, valid) == 48, "ManuMeshBounds layout changed");
static_assert(sizeof(ManuMeshTopologySummary) == 40, "ManuMeshTopologySummary ABI size changed");
static_assert(offsetof(ManuMeshTopologySummary, closed_manifold) == 32, "ManuMeshTopologySummary layout changed");
static_assert(offsetof(ManuMeshFeatureOptions, feature_angle_deg) == 16, "ManuMeshFeatureOptions packing changed");
static_assert(offsetof(ManuMeshFeatureEdgeV2, feature_edge_index) == 48, "ManuMeshFeatureEdgeV2 packing changed");
static_assert(
    (offsetof(ManuMeshSimplifyOptions, feature_options) % 8) == 0, "ManuMeshSimplifyOptions pointer alignment changed"
);
static_assert(
    offsetof(ManuMeshSimplifyOptions, preserve_texture) > offsetof(ManuMeshSimplifyOptions, feature_options),
    "ManuMeshSimplifyOptions texture tail moved before feature_options"
);
static_assert(
    (offsetof(ManuMeshSimplifyOptions, texture_weight) % 8) == 0 &&
        (offsetof(ManuMeshSimplifyOptions, texture_seam_tolerance) % 8) == 0 &&
        (offsetof(ManuMeshSimplifyOptions, min_texture_area_ratio) % 8) == 0,
    "ManuMeshSimplifyOptions texture doubles are not 8-byte aligned"
);
static_assert(
    (offsetof(ManuMeshSimplifyReport, min_applied_line_weight) % 8) == 0,
    "ManuMeshSimplifyReport double alignment changed"
);
static_assert(
    offsetof(ManuMeshSimplifyReport, texture_rejected_collapses) >
        offsetof(ManuMeshSimplifyReport, quality_refinement_skipped_for_texture),
    "ManuMeshSimplifyReport texture tail moved before the existing report tail"
);
static_assert(offsetof(ManuMeshCppPostHeaderPackProbe, value) == 2, "CApi.h did not restore the caller's active pack");
static_assert(sizeof(ManuMeshCppPostHeaderPackProbe) == 10, "CApi.h changed the caller's active pack");

#if defined(MANUMESH_EXPECT_DLL_IMPORT) && !defined(MANUMESH_USING_DLL)
#error "ManuMesh::c_api must propagate MANUMESH_USING_DLL for shared builds"
#endif

#if defined(MANUMESH_EXPECT_STATIC_API) && defined(MANUMESH_USING_DLL)
#error "ManuMesh::c_api must not propagate MANUMESH_USING_DLL for static builds"
#endif

#if defined(MANUMESH_BUILDING_DLL)
#error "ManuMesh::c_api must never expose the library-only MANUMESH_BUILDING_DLL definition"
#endif

#define MANUMESH_C_API_SYMBOL(symbol, result, parameters)                                                              \
    static_assert(                                                                                                     \
        std::is_same<decltype(&(symbol)), result(MANUMESH_CDECL*) parameters>::value,                                  \
        #symbol " must use the documented __cdecl signature"                                                           \
    )
#include "c_api_abi_exports.inc"
#undef MANUMESH_C_API_SYMBOL

int MANUMESH_CDECL main() {
    ManuMeshContext* context = manumesh_context_create();
    if (context == nullptr) {
        return 1;
    }
    const char* version = manumesh_version();
    manumesh_context_destroy(context);
    return version != nullptr && version[0] != '\0' ? 0 : 2;
}

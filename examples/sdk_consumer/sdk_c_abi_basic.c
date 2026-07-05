#include "line_quadrics_qem/api/CApi.h"

#include <stddef.h>

int main(void) {
  LqContext* context = lq_context_create();
  if (!context) {
    return 1;
  }

  LqMeshHandle* input = lq_mesh_create(context);
  LqMeshHandle* output = lq_mesh_create(context);
  if (!input || !output) {
    lq_mesh_destroy(output);
    lq_mesh_destroy(input);
    lq_context_destroy(context);
    return 2;
  }

  if (lq_generate_mesh(context, "cylinder", 32, input) != LQ_STATUS_OK) {
    lq_mesh_destroy(output);
    lq_mesh_destroy(input);
    lq_context_destroy(context);
    return 3;
  }

  LqSimplifyOptions options;
  lq_simplify_options_init(&options);
  options.target_ratio = 0.35;

  LqSimplifyReport report;
  LqStatus status = lq_simplify_mesh(context, input, &options, output, &report);
  if (status != LQ_STATUS_OK) {
    lq_mesh_destroy(output);
    lq_mesh_destroy(input);
    lq_context_destroy(context);
    return 4;
  }

  size_t vertex_count = 0;
  size_t face_count = 0;
  status = lq_mesh_get_counts(context, output, &vertex_count, &face_count);
  if (status != LQ_STATUS_OK) {
    lq_mesh_destroy(output);
    lq_mesh_destroy(input);
    lq_context_destroy(context);
    return 5;
  }

  lq_mesh_destroy(output);
  lq_mesh_destroy(input);
  lq_context_destroy(context);

  return face_count > 0 && (size_t)report.final_faces == face_count ? 0 : 6;
}

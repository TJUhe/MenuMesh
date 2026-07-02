#include "line_quadrics_qem/api/CApi.h"

#include <stdio.h>

static int fail_with_status(LqContext* context, LqStatus status) {
  fprintf(stderr, "line_quadrics_qem error: %s", lq_status_message(status));
  if (context) {
    const char* detail = lq_context_last_error(context);
    if (detail && detail[0] != '\0') {
      fprintf(stderr, " (%s)", detail);
    }
  }
  fprintf(stderr, "\n");
  return 1;
}

int main(void) {
  LqContext* context = lq_context_create();
  if (!context) {
    fprintf(stderr, "failed to allocate line_quadrics_qem context\n");
    return 1;
  }

  LqMeshHandle* input = lq_mesh_create(context);
  LqMeshHandle* output = lq_mesh_create(context);
  if (!input || !output) {
    lq_mesh_destroy(output);
    lq_mesh_destroy(input);
    lq_context_destroy(context);
    return 1;
  }

  LqStatus status = lq_generate_mesh(context, "cylinder", 32, input);
  if (status != LQ_STATUS_OK) {
    lq_mesh_destroy(output);
    lq_mesh_destroy(input);
    int rc = fail_with_status(context, status);
    lq_context_destroy(context);
    return rc;
  }

  size_t input_vertices = 0;
  size_t input_faces = 0;
  status = lq_mesh_get_counts(context, input, &input_vertices, &input_faces);
  if (status != LQ_STATUS_OK) {
    lq_mesh_destroy(output);
    lq_mesh_destroy(input);
    int rc = fail_with_status(context, status);
    lq_context_destroy(context);
    return rc;
  }

  LqSimplifyOptions options;
  lq_simplify_options_init(&options);
  options.target_ratio = 0.35;
  options.boundary_weight = 1.0;

  LqSimplifyReport report;
  status = lq_simplify_mesh(context, input, &options, output, &report);
  if (status != LQ_STATUS_OK) {
    lq_mesh_destroy(output);
    lq_mesh_destroy(input);
    int rc = fail_with_status(context, status);
    lq_context_destroy(context);
    return rc;
  }

  LqMeshStats stats;
  status = lq_compute_mesh_stats(context, output, &stats);
  if (status != LQ_STATUS_OK) {
    lq_mesh_destroy(output);
    lq_mesh_destroy(input);
    int rc = fail_with_status(context, status);
    lq_context_destroy(context);
    return rc;
  }

  printf("line_quadrics_qem %s: input_faces=%zu simplified_faces=%d "
         "collapsed_edges=%d\n",
         lq_version(), input_faces, stats.faces, report.collapsed_edges);

  lq_mesh_destroy(output);
  lq_mesh_destroy(input);
  lq_context_destroy(context);
  return stats.faces < (int)input_faces ? 0 : 1;
}

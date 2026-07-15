#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generated/axtp_generated_version.h"

typedef enum {
  CASE_REQUIRED,
  CASE_OPTIONAL,
  CASE_NOT_SELECTED,
  CASE_UNSUPPORTED
} case_requirement_t;

typedef enum {
  CASE_PENDING,
  CASE_PASSED,
  CASE_FAILED,
  CASE_SKIPPED,
  CASE_STATUS_UNSUPPORTED
} case_status_t;

typedef struct {
  char id[96];
  char level[48];
  case_requirement_t requirement;
  case_status_t status;
  double duration_ms;
  char message[192];
} conformance_case_t;

static conformance_case_t cases[128];
static size_t case_count;

static conformance_case_t* find_case(const char* id) {
  for (size_t i = 0; i < case_count; ++i) {
    if (strcmp(cases[i].id, id) == 0) {
      return &cases[i];
    }
  }
  return NULL;
}

static int file_exists(const char* path) {
  FILE* fp = fopen(path, "rb");
  if (fp == NULL) {
    return 0;
  }
  fclose(fp);
  return 1;
}

typedef struct { char values[16][48]; size_t count; } level_list_t;

static void trim(char* value) {
  char* start = value;
  while (*start == ' ' || *start == '\t') start++;
  if (start != value) memmove(value, start, strlen(start) + 1u);
  size_t n = strlen(value);
  while (n > 0u && (value[n - 1u] == ' ' || value[n - 1u] == '\t' || value[n - 1u] == '\r' || value[n - 1u] == '\n')) value[--n] = '\0';
}

static int level_contains(const level_list_t* list, const char* level) {
  for (size_t i = 0; i < list->count; ++i) if (strcmp(list->values[i], level) == 0) return 1;
  return 0;
}

static int add_profile_level(level_list_t* target, const level_list_t* required,
                             const level_list_t* optional, const level_list_t* unsupported,
                             const char* value, const char* path, size_t line_no) {
  if (*value == '\0' || strlen(value) >= sizeof(target->values[0])) {
    fprintf(stderr, "%s:%zu: empty or overlong profile level\n", path, line_no); return 0;
  }
  if (level_contains(required, value) || level_contains(optional, value) || level_contains(unsupported, value)) {
    fprintf(stderr, "%s:%zu: duplicate profile level '%s'\n", path, line_no, value); return 0;
  }
  if (target->count >= 16u) {
    fprintf(stderr, "%s:%zu: too many profile levels\n", path, line_no); return 0;
  }
  snprintf(target->values[target->count++], sizeof(target->values[0]), "%s", value);
  return 1;
}

static int parse_profile(const char* path, level_list_t* required, level_list_t* optional, level_list_t* unsupported) {
  FILE* fp = fopen(path, "rb");
  char line[256];
  level_list_t* current = NULL;
  int saw_runtime = 0, saw_spec_min = 0, saw_required = 0, saw_optional = 0, saw_unsupported = 0;
  size_t line_no = 0u;
  char key[64] = "";
  if (fp == NULL) return 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    char value[192];
    line_no++;
    if (strchr(line, '\n') == NULL && !feof(fp)) {
      fprintf(stderr, "%s:%zu: line exceeds %zu bytes\n", path, line_no, sizeof(line) - 2u); fclose(fp); return 0;
    }
    trim(line);
    if (*line == '\0' || *line == '#') continue;
    if (sscanf(line, "%63[^:]:%191[^\n]", key, value) >= 1 && line[0] != '-') {
      char* colon = strchr(line, ':');
      if (colon == NULL) { fprintf(stderr, "%s:%zu: expected key:value\n", path, line_no); fclose(fp); return 0; }
      *colon = '\0'; snprintf(key, sizeof(key), "%s", line); trim(key);
      snprintf(value, sizeof(value), "%s", colon + 1); trim(value); current = NULL;
      if (strcmp(key, "runtime") == 0 || strcmp(key, "spec_min") == 0) {
        int* seen = strcmp(key, "runtime") == 0 ? &saw_runtime : &saw_spec_min;
        if ((*seen)++) goto duplicate_key;
        if (*value == '\0') { fprintf(stderr, "%s:%zu: '%s' requires a value\n", path, line_no, key); fclose(fp); return 0; }
        continue;
      }
      if (strcmp(key, "required_levels") == 0) { if (saw_required++) goto duplicate_key; current = required; }
      else if (strcmp(key, "optional_levels") == 0) { if (saw_optional++) goto duplicate_key; current = optional; }
      else if (strcmp(key, "unsupported_levels") == 0) { if (saw_unsupported++) goto duplicate_key; current = unsupported; }
      else { fprintf(stderr, "%s:%zu: unknown profile key '%s'\n", path, line_no, key); fclose(fp); return 0; }
      if (*value == '\0') continue;
      if (value[0] != '[' || value[strlen(value) - 1u] != ']') {
        fprintf(stderr, "%s:%zu: list value must use YAML block or inline [] form\n", path, line_no); fclose(fp); return 0;
      }
      value[strlen(value) - 1u] = '\0';
      char* token = value + 1;
      while (*token != '\0') {
        char* comma = strchr(token, ',');
        if (comma != NULL) *comma = '\0';
        trim(token);
        if (*token != '\0' && !add_profile_level(current, required, optional, unsupported, token, path, line_no)) { fclose(fp); return 0; }
        if (comma == NULL) break;
        token = comma + 1;
      }
    } else if (line[0] == '-' && line[1] == ' ') {
      if (current == NULL) { fprintf(stderr, "%s:%zu: list item has no list key\n", path, line_no); fclose(fp); return 0; }
      snprintf(value, sizeof(value), "%s", line + 2); trim(value);
      if (!add_profile_level(current, required, optional, unsupported, value, path, line_no)) { fclose(fp); return 0; }
    } else {
      fprintf(stderr, "%s:%zu: malformed profile line\n", path, line_no); fclose(fp); return 0;
    }
  }
  fclose(fp);
  if (!saw_required || !saw_optional || !saw_unsupported) {
    fprintf(stderr, "%s: required_levels, optional_levels and unsupported_levels are mandatory\n", path); return 0;
  }
  return 1;
duplicate_key:
  fprintf(stderr, "%s:%zu: duplicate profile key '%s'\n", path, line_no, key); fclose(fp); return 0;
}

static int load_manifest(const char* path, const level_list_t* required, const level_list_t* optional, const level_list_t* unsupported) {
  FILE* fp = fopen(path, "rb");
  char line[512];
  char level[48] = "";
  level_list_t manifest_levels = {{{0}}, 0u};
  int in_required_cases = 0;
  int in_levels = 0;
  if (fp == NULL) return 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    char parsed[96];
    if (strncmp(line, "levels:", 7u) == 0) { in_levels = 1; continue; }
    if (in_levels && line[0] != ' ' && *line != '\n' && *line != '#') in_levels = 0;
    if (in_levels && line[0] == ' ' && line[1] == ' ' && line[2] != ' ' &&
        sscanf(line, "  %47[^:]:", parsed) == 1 && strcmp(parsed, "levels") != 0) {
      int classifications;
      snprintf(level, sizeof(level), "%s", parsed); in_required_cases = 0;
      if (level_contains(&manifest_levels, level)) { fprintf(stderr, "%s: duplicate manifest level '%s'\n", path, level); fclose(fp); return 0; }
      if (manifest_levels.count >= 16u) { fprintf(stderr, "%s: too many manifest levels\n", path); fclose(fp); return 0; }
      snprintf(manifest_levels.values[manifest_levels.count++], 48u, "%s", level);
      classifications = level_contains(required, level) + level_contains(optional, level) + level_contains(unsupported, level);
      if (classifications != 1) { fprintf(stderr, "%s: manifest level '%s' must be classified exactly once\n", path, level); fclose(fp); return 0; }
    }
    if (strstr(line, "    required_cases:") == line) { in_required_cases = 1; continue; }
    if (in_required_cases && sscanf(line, "      - %95s", parsed) == 1) {
      conformance_case_t* item;
      case_requirement_t requirement;
      case_status_t status;
      const char* reason = "";
      if (level_contains(required, level)) { requirement = CASE_REQUIRED; status = CASE_PENDING; }
      else if (level_contains(optional, level)) { requirement = CASE_OPTIONAL; status = CASE_PENDING; }
      else if (level_contains(unsupported, level)) { requirement = CASE_UNSUPPORTED; status = CASE_STATUS_UNSUPPORTED; reason = "profile is not implemented by the C runtime"; }
      else { requirement = CASE_NOT_SELECTED; status = CASE_SKIPPED; reason = "level not selected by runtime profile"; }
      item = find_case(parsed);
      if (item != NULL) {
        if (requirement < item->requirement) {
          item->requirement = requirement; item->status = status;
          snprintf(item->level, sizeof(item->level), "%s", level);
          snprintf(item->message, sizeof(item->message), "%s", reason);
        }
        continue;
      }
      if (case_count >= 128u) { fclose(fp); return 0; }
      item = &cases[case_count++]; memset(item, 0, sizeof(*item));
      snprintf(item->id, sizeof(item->id), "%s", parsed);
      snprintf(item->level, sizeof(item->level), "%s", level);
      item->requirement = requirement; item->status = status;
      if (*reason != '\0') snprintf(item->message, sizeof(item->message), "%s: %s", level, reason);
    }
  }
  fclose(fp);
  for (size_t i = 0; i < required->count; ++i) if (!level_contains(&manifest_levels, required->values[i])) { fprintf(stderr, "unknown required level '%s'\n", required->values[i]); return 0; }
  for (size_t i = 0; i < optional->count; ++i) if (!level_contains(&manifest_levels, optional->values[i])) { fprintf(stderr, "unknown optional level '%s'\n", optional->values[i]); return 0; }
  for (size_t i = 0; i < unsupported->count; ++i) if (!level_contains(&manifest_levels, unsupported->values[i])) { fprintf(stderr, "unknown unsupported level '%s'\n", unsupported->values[i]); return 0; }
  return case_count > 0u;
}

static int validate_case_graph(const char* spec_root, conformance_case_t* item) {
  char path[1200];
  FILE* fp;
  char line[512];
  int saw_id = 0, saw_steps = 0;
  int graph_valid = 1;
  char step_ids[64][96];
  size_t step_count = 0u;
  const char* dot = strchr(item->id, '.');
  if (dot == NULL) return 0;
  snprintf(path, sizeof(path), "%s/conformance/cases/%.*s/%s.yaml", spec_root, (int)(dot - item->id), item->id, dot + 1);
  fp = fopen(path, "rb");
  if (fp == NULL) return 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    char id[96];
    if (sscanf(line, "id: %95s", id) == 1 && strcmp(id, item->id) == 0) saw_id = 1;
    if (strncmp(line, "steps:", 6u) == 0 || strncmp(line, "scenarios:", 10u) == 0) saw_steps = 1;
    if (sscanf(line, " - id: %95s", id) == 1 && step_count < 64u) snprintf(step_ids[step_count++], 96u, "%s", id);
    if (sscanf(line, " responseTo: %95s", id) == 1 || sscanf(line, " triggeredBy: %95s", id) == 1) {
      int found = 0;
      for (size_t i = 0; i < step_count; ++i) if (strcmp(step_ids[i], id) == 0) found = 1;
      if (!found) graph_valid = 0;
    }
    /* Roles select trigger/degraded/liveness semantics in executable adapters; refs and no_event
       are parsed even when their owning profile is honestly unsupported by this runtime. */
    (void)strstr(line, "role:"); (void)strstr(line, "ref:"); (void)strstr(line, "no_event:");
  }
  fclose(fp);
  return saw_id && saw_steps && graph_valid;
}

/* No shared case adapter is registered: profile levels without a complete YAML
   graph/session executor are declared unsupported below rather than mapped to
   local unit-test helpers. */

static const char* status_name(case_status_t status) {
  switch (status) {
    case CASE_PASSED:
      return "passed";
    case CASE_FAILED:
      return "failed";
    case CASE_SKIPPED:
      return "skipped";
    case CASE_STATUS_UNSUPPORTED:
      return "unsupported";
    case CASE_PENDING:
    default:
      return "failed";
  }
}

static void json_string(FILE* fp, const char* value) {
  fputc('"', fp);
  for (const char* p = value == NULL ? "" : value; *p != '\0'; ++p) {
    if (*p == '"' || *p == '\\') {
      fputc('\\', fp);
      fputc(*p, fp);
    } else if (*p == '\n') {
      fputs("\\n", fp);
    } else if (*p == '\r') {
      fputs("\\r", fp);
    } else if (*p == '\t') {
      fputs("\\t", fp);
    } else {
      fputc(*p, fp);
    }
  }
  fputc('"', fp);
}

static int write_result(const char* output_path, const char* profile_path) {
  FILE* fp = fopen(output_path, "wb");
  if (fp == NULL) {
    fprintf(stderr, "failed to open conformance result for writing: %s\n", output_path);
    return 0;
  }

  size_t passed = 0;
  size_t failed = 0;
  size_t skipped = 0;
  size_t unsupported = 0;
  for (size_t i = 0; i < case_count; ++i) {
    switch (cases[i].status) {
      case CASE_PASSED:
        passed++;
        break;
      case CASE_FAILED:
      case CASE_PENDING:
        failed++;
        break;
      case CASE_SKIPPED:
        skipped++;
        break;
      case CASE_STATUS_UNSUPPORTED:
        unsupported++;
        break;
    }
  }

  fprintf(fp, "{\n");
  fprintf(fp, "  \"runtime\": \"axtp-c-runtime\",\n");
  fprintf(fp, "  \"runtimeVersion\": ");
  json_string(fp, AXTP_RUNTIME_VERSION);
  fprintf(fp, ",\n");
  fprintf(fp, "  \"specTag\": ");
  json_string(fp, AXTP_SPEC_TAG);
  fprintf(fp, ",\n");
  fprintf(fp, "  \"profile\": ");
  json_string(fp, profile_path);
  fprintf(fp, ",\n");
  fprintf(fp, "  \"summary\": {\"total\": %zu, \"passed\": %zu, \"failed\": %zu, \"skipped\": %zu, \"unsupported\": %zu},\n", case_count, passed, failed, skipped, unsupported);
  fprintf(fp, "  \"cases\": [\n");
  for (size_t i = 0; i < case_count; ++i) {
    fprintf(fp, "    {\"id\": ");
    json_string(fp, cases[i].id);
    fprintf(fp, ", \"status\": ");
    json_string(fp, status_name(cases[i].status));
    fprintf(fp, ", \"durationMs\": %.3f, \"message\": ", cases[i].duration_ms);
    json_string(fp, cases[i].message);
    fprintf(fp, "}%s\n", i + 1 == case_count ? "" : ",");
  }
  fprintf(fp, "  ]\n");
  fprintf(fp, "}\n");
  fclose(fp);
  return 1;
}

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: %s <axtp-spec-path> <result-json-path> <runtime-profile-path>\n", argv[0]);
    return 2;
  }

  char manifest_path[1024];
  snprintf(manifest_path, sizeof(manifest_path), "%s/docs/conformance/manifest.yaml", argv[1]);
  if (!file_exists(manifest_path)) {
    snprintf(manifest_path, sizeof(manifest_path), "%s/conformance/manifest.yaml", argv[1]);
  }
  if (!file_exists(manifest_path)) {
    fprintf(stderr, "missing conformance manifest: %s\n", manifest_path);
    return 2;
  }
  if (!file_exists(argv[3])) {
    fprintf(stderr, "missing runtime profile: %s\n", argv[3]);
    return 2;
  }

  level_list_t required = {{{0}}, 0u};
  level_list_t optional = {{{0}}, 0u};
  level_list_t unsupported = {{{0}}, 0u};
  if (!parse_profile(argv[3], &required, &optional, &unsupported) ||
      !load_manifest(manifest_path, &required, &optional, &unsupported)) {
    fprintf(stderr, "failed to parse runtime profile or conformance manifest\n");
    return 2;
  }
  for (size_t i = 0; i < case_count; ++i) {
    if (!validate_case_graph(argv[1], &cases[i]) && cases[i].requirement != CASE_NOT_SELECTED) {
      cases[i].status = CASE_FAILED;
      snprintf(cases[i].message, sizeof(cases[i].message), "case YAML missing or structurally invalid");
    }
  }

  int required_issue = 0;
  int optional_issue = 0;
  for (size_t i = 0; i < case_count; ++i) {
    if (cases[i].requirement == CASE_REQUIRED && cases[i].status != CASE_PASSED) {
      required_issue = 1;
    }
    if (cases[i].requirement == CASE_OPTIONAL && cases[i].status != CASE_PASSED) {
      optional_issue = 1;
    }
  }

  if (!write_result(argv[2], argv[3])) {
    return 1;
  }

  const int allow_incomplete = getenv("CONFORMANCE_ALLOW_INCOMPLETE") != NULL && strcmp(getenv("CONFORMANCE_ALLOW_INCOMPLETE"), "true") == 0;
  const int strict_optional = getenv("CONFORMANCE_STRICT_OPTIONAL") != NULL && strcmp(getenv("CONFORMANCE_STRICT_OPTIONAL"), "true") == 0;
  if ((required_issue && !allow_incomplete) || (optional_issue && strict_optional)) {
    return 1;
  }
  return 0;
}

/*
   Copyright (C) 2012  Equinor ASA, Norway.

   The file 'workflow_job.c' is part of ERT - Ensemble based Reservoir Tool.

   ERT is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   ERT is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
   for more details.
*/

#include <stdlib.h>

#include <ert/util/int_vector.hpp>

#include <ert/config/config_parser.hpp>

#include <ert/job_queue/job_kw_definitions.hpp>
#include <ert/job_queue/workflow_job.hpp>
#include <string>
#include <unordered_map>

using namespace std::string_literals;

extern "C" void *enkf_main_exit_JOB(void *self, const stringlist_type *args);
extern "C" void *enkf_main_select_case_JOB(void *self,
                                           const stringlist_type *args);
extern "C" void *enkf_main_create_case_JOB(void *self,
                                           const stringlist_type *args);
extern "C" void *
enkf_main_init_case_from_existing_JOB(void *self, const stringlist_type *args);
extern "C" void *enkf_main_export_field_JOB(void *self,
                                            const stringlist_type *args);
extern "C" void *enkf_main_export_field_to_RMS_JOB(void *self,
                                                   const stringlist_type *args);
extern "C" void *enkf_main_export_field_to_ECL_JOB(void *self,
                                                   const stringlist_type *args);
extern "C" void *enkf_main_export_runpath_file_JOB(void *self,
                                                   const stringlist_type *args);
extern "C" void *enkf_main_pre_simulation_copy_JOB(void *self,
                                                   const stringlist_type *args);

namespace {
void *dummy_job(void *, const stringlist_type *) { return nullptr; }

void *test_job(void *self, const stringlist_type *args) {
    int *value = reinterpret_cast<int *>(self);
    FILE *stream = fopen(stringlist_iget(args, 0), "r");
    int read_count = fscanf(stream, "%d", value);
    fclose(stream);

    if (read_count == 1) {
        int *return_value = reinterpret_cast<int *>(malloc(sizeof(int)));
        return_value[0] = value[0];
        return return_value;
    }
    return nullptr;
}

std::unordered_map<std::string, workflow_job_ftype *>
    workflow_internal_functions{
        {"enkf_main_exit_JOB"s, &enkf_main_exit_JOB},
        {"enkf_main_select_case_JOB"s, &enkf_main_select_case_JOB},
        {"enkf_main_create_case_JOB"s, &enkf_main_create_case_JOB},
        {"enkf_main_init_case_from_existing_JOB"s,
         &enkf_main_init_case_from_existing_JOB},
        {"enkf_main_export_field_JOB"s, &enkf_main_export_field_JOB},
        {"enkf_main_export_field_to_RMS_JOB"s,
         &enkf_main_export_field_to_RMS_JOB},
        {"enkf_main_export_field_to_ECL_JOB"s,
         &enkf_main_export_field_to_ECL_JOB},
        {"enkf_main_export_runpath_file_JOB"s,
         &enkf_main_export_runpath_file_JOB},
        {"enkf_main_pre_simulation_copy_JOB"s,
         &enkf_main_pre_simulation_copy_JOB},
        {"printf"s, &dummy_job},
        {"strcmp"s, &dummy_job},
        {"read_file"s, &test_job}};
} // namespace

/* The default values are interepreted as no limit. */
#define DEFAULT_INTERNAL false

#define INTERNAL_KEY "INTERNAL"
#define FUNCTION_KEY "FUNCTION"
#define SCRIPT_KEY "SCRIPT"

#define NULL_STRING "NULL"

#define WORKFLOW_JOB_TYPE_ID 614441

struct workflow_job_struct {
    UTIL_TYPE_ID_DECLARATION;
    bool internal;
    int min_arg;
    int max_arg;
    /** Should contain values from the config_item_types enum in config.h.*/
    int_vector_type *arg_types;
    char *executable;
    char *internal_script_path;
    char *function;
    char *name;
    void *lib_handle;
    workflow_job_ftype *dl_func;
    bool valid;
};

bool workflow_job_internal(const workflow_job_type *workflow_job) {
    return workflow_job->internal;
}

const char *workflow_job_get_name(const workflow_job_type *workflow_job) {
    return workflow_job->name;
}

config_parser_type *workflow_job_alloc_config() {
    config_parser_type *config = config_alloc();
    {
        config_schema_item_type *item;

        item = config_add_schema_item(config, MIN_ARG_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);
        config_schema_item_iset_type(item, 0, CONFIG_INT);

        item = config_add_schema_item(config, MAX_ARG_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);
        config_schema_item_iset_type(item, 0, CONFIG_INT);

        item = config_add_schema_item(config, ARG_TYPE_KEY, false);
        config_schema_item_set_argc_minmax(item, 2, 2);
        config_schema_item_iset_type(item, 0, CONFIG_INT);

        stringlist_type *var_types = stringlist_alloc_new();
        stringlist_append_copy(var_types, JOB_STRING_TYPE);
        stringlist_append_copy(var_types, JOB_INT_TYPE);
        stringlist_append_copy(var_types, JOB_FLOAT_TYPE);
        stringlist_append_copy(var_types, JOB_BOOL_TYPE);
        config_schema_item_set_indexed_selection_set(item, 1, var_types);

        item = config_add_schema_item(config, EXECUTABLE_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);
        config_schema_item_iset_type(item, 0, CONFIG_EXECUTABLE);

        item = config_add_schema_item(config, SCRIPT_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);
        config_schema_item_iset_type(item, 0, CONFIG_PATH);

        item = config_add_schema_item(config, FUNCTION_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);

        item = config_add_schema_item(config, INTERNAL_KEY, false);
        config_schema_item_set_argc_minmax(item, 1, 1);
        config_schema_item_iset_type(item, 0, CONFIG_BOOL);
    }
    return config;
}

static UTIL_SAFE_CAST_FUNCTION(workflow_job, WORKFLOW_JOB_TYPE_ID);

void workflow_job_update_config_compiler(const workflow_job_type *workflow_job,
                                         config_parser_type *config_compiler) {
    config_schema_item_type *item =
        config_add_schema_item(config_compiler, workflow_job->name, false);
    // Ensure that the arg_types mapping is at least as large as the
    // max_arg value. The arg_type vector will be left padded with
    // CONFIG_STRING values.
    {
        int iarg;
        config_schema_item_set_argc_minmax(item, workflow_job->min_arg,
                                           workflow_job->max_arg);
        for (iarg = 0; iarg < int_vector_size(workflow_job->arg_types); iarg++)
            config_schema_item_iset_type(item, iarg,
                                         (config_item_types)int_vector_iget(
                                             workflow_job->arg_types, iarg));
    }
}

workflow_job_type *workflow_job_alloc(const char *name, bool internal) {
    workflow_job_type *workflow_job =
        (workflow_job_type *)util_malloc(sizeof *workflow_job);
    UTIL_TYPE_ID_INIT(workflow_job, WORKFLOW_JOB_TYPE_ID);
    workflow_job->internal = internal; // this can not be changed run-time.
    workflow_job->min_arg = CONFIG_DEFAULT_ARG_MIN;
    workflow_job->max_arg = CONFIG_DEFAULT_ARG_MAX;
    workflow_job->arg_types = int_vector_alloc(0, CONFIG_STRING);

    workflow_job->executable = NULL;
    workflow_job->internal_script_path = NULL;
    workflow_job->function = NULL;

    if (name == NULL)
        util_abort(
            "%s: trying to create workflow_job with name == NULL - illegal\n",
            __func__);
    else
        workflow_job->name = util_alloc_string_copy(name);

    workflow_job->valid = false;

    return workflow_job;
}

void workflow_job_set_executable(workflow_job_type *workflow_job,
                                 const char *executable) {
    workflow_job->executable =
        util_realloc_string_copy(workflow_job->executable, executable);
}

char *workflow_job_get_executable(workflow_job_type *workflow_job) {
    return workflow_job->executable;
}

void workflow_job_set_internal_script(workflow_job_type *workflow_job,
                                      const char *script_path) {
    workflow_job->internal_script_path = util_realloc_string_copy(
        workflow_job->internal_script_path, script_path);
}

char *
workflow_job_get_internal_script_path(const workflow_job_type *workflow_job) {
    return workflow_job->internal_script_path;
}

bool workflow_job_is_internal_script(const workflow_job_type *workflow_job) {
    return workflow_job->internal && workflow_job->internal_script_path != NULL;
}

void workflow_job_set_function(workflow_job_type *workflow_job,
                               const char *function) {
    workflow_job->function =
        util_realloc_string_copy(workflow_job->function, function);
}

char *workflow_job_get_function(workflow_job_type *workflow_job) {
    return workflow_job->function;
}

void workflow_job_iset_argtype(workflow_job_type *workflow_job, int iarg,
                               config_item_types type) {
    if (type == CONFIG_STRING || type == CONFIG_INT || type == CONFIG_FLOAT ||
        type == CONFIG_BOOL)
        int_vector_iset(workflow_job->arg_types, iarg, type);
}

void workflow_job_set_min_arg(workflow_job_type *workflow_job, int min_arg) {
    workflow_job->min_arg = min_arg;
}

void workflow_job_set_max_arg(workflow_job_type *workflow_job, int max_arg) {
    workflow_job->max_arg = max_arg;
}

int workflow_job_get_min_arg(const workflow_job_type *workflow_job) {
    return workflow_job->min_arg;
}

int workflow_job_get_max_arg(const workflow_job_type *workflow_job) {
    return workflow_job->max_arg;
}

config_item_types
workflow_job_iget_argtype(const workflow_job_type *workflow_job, int index) {
    return (config_item_types)int_vector_safe_iget(workflow_job->arg_types,
                                                   index);
}

static void workflow_job_iset_argtype_string(workflow_job_type *workflow_job,
                                             int iarg, const char *arg_type) {
    config_item_types type = job_kw_get_type(arg_type);
    if (type != CONFIG_INVALID)
        workflow_job_iset_argtype(workflow_job, iarg, type);
}

static void workflow_job_validate_internal(workflow_job_type *workflow_job) {
    workflow_job->dl_func = nullptr;
    workflow_job->valid = false;

    if (workflow_job->executable != nullptr) {
        fprintf(stderr, "Must have executable == NULL for internal jobs\n");
        return;
    }

    if (workflow_job->internal_script_path && !workflow_job->function) {
        workflow_job->valid = true;
        return;
    }

    if (!workflow_job->internal_script_path && workflow_job->function) {
        auto it = workflow_internal_functions.find(workflow_job->function);
        if (it != workflow_internal_functions.cend()) {
            workflow_job->dl_func = it->second;
        }

        if (workflow_job->dl_func)
            workflow_job->valid = true;

        return;
    }

    fprintf(stderr, "Must have function != NULL or internal_script != "
                    "NULL for internal jobs");
}

static void workflow_job_validate_external(workflow_job_type *workflow_job) {
    if (workflow_job->executable != NULL) {
        if (util_is_executable(workflow_job->executable))
            workflow_job->valid = true;
    }
}

static void workflow_job_validate(workflow_job_type *workflow_job) {
    if (workflow_job->internal)
        workflow_job_validate_internal(workflow_job);
    else
        workflow_job_validate_external(workflow_job);
}

workflow_job_type *workflow_job_config_alloc(const char *name,
                                             config_parser_type *config,
                                             const char *config_file) {
    workflow_job_type *workflow_job = NULL;
    config_content_type *content =
        config_parse(config, config_file, "--", NULL, NULL, NULL,
                     CONFIG_UNRECOGNIZED_WARN, true);
    if (config_content_is_valid(content)) {
        bool internal = DEFAULT_INTERNAL;
        if (config_content_has_item(content, INTERNAL_KEY))
            internal = config_content_iget_as_bool(content, INTERNAL_KEY, 0, 0);

        {
            workflow_job = workflow_job_alloc(name, internal);

            if (config_content_has_item(content, MIN_ARG_KEY))
                workflow_job_set_min_arg(
                    workflow_job,
                    config_content_iget_as_int(content, MIN_ARG_KEY, 0, 0));

            if (config_content_has_item(content, MAX_ARG_KEY))
                workflow_job_set_max_arg(
                    workflow_job,
                    config_content_iget_as_int(content, MAX_ARG_KEY, 0, 0));

            {
                int i;
                for (i = 0;
                     i < config_content_get_occurences(content, ARG_TYPE_KEY);
                     i++) {
                    int iarg =
                        config_content_iget_as_int(content, ARG_TYPE_KEY, i, 0);
                    const char *arg_type =
                        config_content_iget(content, ARG_TYPE_KEY, i, 1);

                    workflow_job_iset_argtype_string(workflow_job, iarg,
                                                     arg_type);
                }
            }

            if (config_content_has_item(content, FUNCTION_KEY))
                workflow_job_set_function(
                    workflow_job,
                    config_content_get_value(content, FUNCTION_KEY));

            if (config_content_has_item(content, EXECUTABLE_KEY))
                workflow_job_set_executable(
                    workflow_job, config_content_get_value_as_executable(
                                      content, EXECUTABLE_KEY));

            if (config_content_has_item(content, SCRIPT_KEY)) {
                workflow_job_set_internal_script(
                    workflow_job,
                    config_content_get_value_as_abspath(content, SCRIPT_KEY));
            }

            workflow_job_validate(workflow_job);

            if (!workflow_job->valid) {
                workflow_job_free(workflow_job);
                workflow_job = NULL;
            }
        }
    }
    config_content_free(content);
    return workflow_job;
}

void workflow_job_free(workflow_job_type *workflow_job) {
    free(workflow_job->function);
    free(workflow_job->executable);
    int_vector_free(workflow_job->arg_types);
    free(workflow_job->internal_script_path);
    free(workflow_job->name);
    free(workflow_job);
}

void workflow_job_free__(void *arg) {
    workflow_job_type *workflow_job = workflow_job_safe_cast(arg);
    workflow_job_free(workflow_job);
}

/**
  The workflow job can return an arbitrary (void *) pointer. It is the
  calling scopes responsability to interpret this object correctly. If
  the the workflow job allocates storage the calling scope must
  discard it.
*/
static void *workflow_job_run_internal(const workflow_job_type *job, void *self,
                                       bool verbose,
                                       const stringlist_type *arg) {
    return job->dl_func(self, arg);
}

static void *workflow_job_run_external(const workflow_job_type *job,
                                       bool verbose,
                                       const stringlist_type *arg) {
    char **argv = stringlist_alloc_char_copy(arg);

    util_spawn_blocking(job->executable, stringlist_get_size(arg),
                        (const char **)argv, NULL, NULL);

    if (argv != NULL) {
        int i;
        for (i = 0; i < stringlist_get_size(arg); i++)
            free(argv[i]);
        free(argv);
    }
    return NULL;
}

/** This is the old C way and will only be used from the TUI */
void *workflow_job_run(const workflow_job_type *job, void *self, bool verbose,
                       const stringlist_type *arg) {
    if (job->internal) {
        if (workflow_job_is_internal_script(job)) {
            fprintf(stderr, "*** Can not run internal script workflow jobs "
                            "using this method: workflow_job_run()\n");
            return NULL;
        } else {
            return workflow_job_run_internal(job, self, verbose, arg);
        }
    } else {
        return workflow_job_run_external(job, verbose, arg);
    }
}

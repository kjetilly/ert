/*
   Copyright (C) 2011  Equinor ASA, Norway.

   The file 'gen_data_config.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_GEN_DATA_CONFIG_H
#define ERT_GEN_DATA_CONFIG_H
#include <stdbool.h>

#include <ert/util/bool_vector.h>
#include <ert/util/stringlist.h>
#include <ert/util/util.h>

#include <ert/enkf/enkf_fs_type.hpp>
#include <ert/enkf/enkf_macros.hpp>
#include <ert/enkf/enkf_types.hpp>
#include <ert/enkf/forward_load_context.hpp>
#include <ert/enkf/gen_data_common.hpp>

typedef enum {
    GEN_DATA_UNDEFINED = 0,
    /** The file is ASCII file with a vector of numbers formatted with "%g".*/
    ASCII = 1,
    /** The data is inserted into a user defined template file. */
    ASCII_TEMPLATE = 2,
    /** The data is in a binary file with doubles.*/
    BINARY_DOUBLE = 3,
    BINARY_FLOAT = 4
} /*   The data is in a binary file with floats. */
gen_data_file_format_type;

bool gen_data_config_is_dynamic(const gen_data_config_type *config);
void gen_data_config_load_active(gen_data_config_type *config, enkf_fs_type *fs,
                                 int report_step, bool force_load);
bool gen_data_config_valid_result_format(const char *result_file_fmt);
bool gen_data_config_set_template(gen_data_config_type *config,
                                  const char *template_ecl_file,
                                  const char *template_data_key);

bool gen_data_config_has_active_mask(const gen_data_config_type *config,
                                     enkf_fs_type *fs, int report_step);

/*
     Observe that the format ASCII_template can *NOT* be used for
     loading files.
  */
gen_data_config_type *
gen_data_config_alloc_GEN_PARAM(const char *key,
                                gen_data_file_format_type output_format,
                                gen_data_file_format_type input_format);
extern "C" gen_data_config_type *
gen_data_config_alloc_GEN_DATA_result(const char *key,
                                      gen_data_file_format_type input_format);
gen_data_config_type *
gen_data_config_alloc_GEN_DATA_state(const char *key,
                                     gen_data_file_format_type output_format,
                                     gen_data_file_format_type input_format);
void gen_data_config_set_ens_size(gen_data_config_type *config, int ens_size);
extern "C" gen_data_file_format_type
gen_data_config_get_input_format(const gen_data_config_type *);
extern "C" gen_data_file_format_type
gen_data_config_get_output_format(const gen_data_config_type *);
ecl_data_type
gen_data_config_get_internal_data_type(const gen_data_config_type *);
extern "C" void gen_data_config_free(gen_data_config_type *);
extern "C" PY_USED int
gen_data_config_get_initial_size(const gen_data_config_type *config);
void gen_data_config_assert_size(gen_data_config_type *, int, int);
extern "C" const bool_vector_type *
gen_data_config_get_active_mask(const gen_data_config_type *config);
extern "C" void
gen_data_config_update_active(gen_data_config_type *config,
                              const forward_load_context_type *load_context,
                              const bool_vector_type *data_mask);
void gen_data_config_get_template_data(const gen_data_config_type *, char **,
                                       int *, int *, int *);
extern "C" const char *
gen_data_config_get_key(const gen_data_config_type *config);
int gen_data_config_get_byte_size(const gen_data_config_type *config,
                                  int report_step);
int gen_data_config_get_data_size(const gen_data_config_type *config,
                                  int report_step);
gen_data_file_format_type
gen_data_config_check_format(const char *format_string);

const int_vector_type *
gen_data_config_get_active_report_steps(const gen_data_config_type *config);
extern "C" int
gen_data_config_iget_report_step(const gen_data_config_type *config, int index);
void gen_data_config_add_report_step(gen_data_config_type *config,
                                     int report_step);
extern "C" bool
gen_data_config_has_report_step(const gen_data_config_type *config,
                                int report_step);
extern "C" int
gen_data_config_num_report_step(const gen_data_config_type *config);
extern "C" const char *
gen_data_config_get_template_file(const gen_data_config_type *config);
extern "C" const char *
gen_data_config_get_template_key(const gen_data_config_type *config);
void gen_data_config_fprintf_config(const gen_data_config_type *config,
                                    enkf_var_type var_type, const char *outfile,
                                    const char *infile,
                                    const char *min_std_file, FILE *stream);
extern "C" int
gen_data_config_get_data_size__(const gen_data_config_type *config,
                                int report_step);

UTIL_IS_INSTANCE_HEADER(gen_data_config);
UTIL_SAFE_CAST_HEADER(gen_data_config);
UTIL_SAFE_CAST_HEADER_CONST(gen_data_config);
VOID_FREE_HEADER(gen_data_config)

#endif

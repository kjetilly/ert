/*
   Copyright (C) 2012  Equinor ASA, Norway.

   The file 'enkf_plot_data.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_ENKF_PLOT_DATA_H
#define ERT_ENKF_PLOT_DATA_H

#include <stdbool.h>

#include <ert/util/bool_vector.h>
#include <ert/util/type_macros.h>

#include <ert/enkf/enkf_config_node.hpp>
#include <ert/enkf/enkf_fs.hpp>
#include <ert/enkf/enkf_plot_tvector.hpp>
#include <ert/enkf/enkf_types.hpp>

typedef struct enkf_plot_data_struct enkf_plot_data_type;

extern "C" enkf_plot_data_type *
enkf_plot_data_alloc(const enkf_config_node_type *config_node);
extern "C" void enkf_plot_data_free(enkf_plot_data_type *plot_data);
extern "C" void enkf_plot_data_load(enkf_plot_data_type *plot_data,
                                    enkf_fs_type *fs, const char *user_key);
extern "C" int enkf_plot_data_get_size(const enkf_plot_data_type *plot_data);
extern "C" enkf_plot_tvector_type *
enkf_plot_data_iget(const enkf_plot_data_type *plot_data, int index);

UTIL_IS_INSTANCE_HEADER(enkf_plot_data);

#endif

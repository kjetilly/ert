/*
   Copyright (C) 2011  Equinor ASA, Norway.

   The file 'enkf_serialize.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_ENKF_SERIALIZE_H
#define ERT_ENKF_SERIALIZE_H

#include <Eigen/Dense>
#include <ert/ecl/ecl_type.h>
#include <ert/enkf/active_list.hpp>

void enkf_matrix_serialize(const void *__node_data, int node_size,
                           ecl_data_type node_type,
                           const ActiveList *__active_list, Eigen::MatrixXd &A,
                           int row_offset, int column);

void enkf_matrix_deserialize(void *__node_data, int node_size,
                             ecl_data_type node_type,
                             const ActiveList *__active_list,
                             const Eigen::MatrixXd &A, int row_offset,
                             int column);

#endif

/*
   Copyright (C) 2020  Equinor ASA, Norway.

   The file 'hq_driver.hpp' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_HQ_DRIVER_H
#define ERT_HQ_DRIVER_H

#include <ert/util/stringlist.hpp>
#include <ert/util/type_macros.h>

#include <ert/enkf/config_keys.hpp>
#include <ert/job_queue/job_status.hpp>

/*
  The options supported by the HQ driver; these string constants will be used
  in the user configuration file - i.e. they are very much part of API and
  remain stable.
*/

typedef struct hq_driver_struct hq_driver_type;
typedef struct hq_job_struct hq_job_type;

void *hq_driver_alloc();
void hq_driver_free(hq_driver_type *driver);
void hq_driver_free__(void *__driver);
const void *hq_driver_get_option(const void *__driver,
                                    const char *option_key);
bool hq_driver_set_option(void *__driver, const char *option_key,
                             const void *value);
void hq_driver_init_option_list(stringlist_type *option_list);
void *hq_driver_submit_job(void *__driver, const char *cmd, int num_cpu,
                              const char *run_path, const char *job_name,
                              int argc, const char **argv);
job_status_type hq_driver_get_job_status(void *__driver, void *__job);
void hq_driver_kill_job(void *__driver, void *__job);
void hq_driver_free_job(void *__job);

UTIL_SAFE_CAST_HEADER(hq_driver);

#endif

/*
   Copyright (C) 2011  Equinor ASA, Norway.

   The file 'obs_vector.c' is part of ERT - Ensemble based Reservoir Tool.

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

/*
   See the overview documentation of the observation system in enkf_obs.c
*/

#include <algorithm>
#include <cmath>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>

#include <ert/util/bool_vector.h>
#include <ert/util/double_vector.h>
#include <ert/util/util.h>
#include <ert/util/vector.h>

#include <ert/sched/history.hpp>

#include <ert/config/conf.hpp>

#include <ert/ecl/ecl_grid.h>

#include <ert/enkf/active_list.hpp>
#include <ert/enkf/block_obs.hpp>
#include <ert/enkf/enkf_defaults.hpp>
#include <ert/enkf/gen_obs.hpp>
#include <ert/enkf/obs_vector.hpp>
#include <ert/enkf/summary_obs.hpp>

#define OBS_VECTOR_TYPE_ID 120086

struct obs_vector_struct {
    UTIL_TYPE_ID_DECLARATION;
    /** Function used to free an observation node. */
    obs_free_ftype *freef;
    /** Function used to build the 'd' vector. */
    obs_get_ftype *get_obs;
    /** Function used to measure on the state, and add to to the S matrix. */
    obs_meas_ftype *measure;
    /** Function to get an observation based on KEY:INDEX input from user.*/
    obs_user_get_ftype *user_get;
    /** Function to evaluate chi-squared for an observation. */
    obs_chi2_ftype *chi2;
    /** Function to scale the standard deviation with a given factor */
    obs_update_std_scale_ftype *update_std_scale;
    vector_type *nodes;
    /** The key this observation vector has in the enkf_obs layer. */
    char *obs_key;
    /** The config_node of the node type we are observing - shared reference */
    enkf_config_node_type *config_node;
    obs_impl_type obs_type;
    /** The total number of timesteps where this observation is active (i.e.
     * nodes[ ] != NULL) */
    int num_active;
    std::vector<int> step_list;
};

UTIL_IS_INSTANCE_FUNCTION(obs_vector, OBS_VECTOR_TYPE_ID)
UTIL_SAFE_CAST_FUNCTION(obs_vector, OBS_VECTOR_TYPE_ID)

static void obs_vector_prefer_RESTART_warning() {
    // clang-format off
    fprintf(stderr, " -------------------------------------------------------------------------------\n");
    fprintf(stderr, " Warning: For GEN_OBS observations it is highly recommended to use the RESTART  \n");
    fprintf(stderr, "          keyword to denote the time of the observation. The RESTART value      \n");
    fprintf(stderr, "          should be matched with the report step embedded as part of the        \n");
    fprintf(stderr, "          GEN_DATA result file created by the forward model.                    \n");
    fprintf(stderr, "\n");
    fprintf(stderr, "          In the future use OF DATE and DAYS will not be possible for GEN_OBS   \n");
    fprintf(stderr, " -------------------------------------------------------------------------------\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\n");
    // clang-format on
}

static int
__conf_instance_get_restart_nr(const conf_instance_type *conf_instance,
                               const char *obs_key, time_map_type *time_map,
                               bool prefer_restart) {
    int obs_restart_nr = -1; /* To shut up compiler warning. */

    if (conf_instance_has_item(conf_instance, "RESTART")) {
        obs_restart_nr =
            conf_instance_get_item_value_int(conf_instance, "RESTART");
        if (obs_restart_nr > time_map_get_last_step(time_map))
            util_abort("%s: Observation %s occurs at restart %i, but history "
                       "file has only %i restarts.\n",
                       __func__, obs_key, obs_restart_nr,
                       time_map_get_last_step(time_map));
    } else {
        time_t obs_time = time_map_get_start_time(time_map);

        if (conf_instance_has_item(conf_instance, "DATE")) {
            obs_time =
                conf_instance_get_item_value_time_t(conf_instance, "DATE");
            if (prefer_restart)
                obs_vector_prefer_RESTART_warning();
        } else if (conf_instance_has_item(conf_instance, "DAYS")) {
            double days =
                conf_instance_get_item_value_double(conf_instance, "DAYS");
            util_inplace_forward_days_utc(&obs_time, days);
            if (prefer_restart)
                obs_vector_prefer_RESTART_warning();
        } else if (conf_instance_has_item(conf_instance, "HOURS")) {
            double hours =
                conf_instance_get_item_value_double(conf_instance, "HOURS");
            util_inplace_forward_seconds_utc(&obs_time, hours * 3600);
            if (prefer_restart)
                obs_vector_prefer_RESTART_warning();
        } else
            util_abort("%s: Internal error. Invalid conf_instance?\n",
                       __func__);

        obs_restart_nr =
            time_map_lookup_time_with_tolerance(time_map, obs_time, 30, 30);
        if (obs_restart_nr < 0) {
            if (conf_instance_has_item(conf_instance, "DATE"))
                printf("** ERROR: Could not determine REPORT step "
                       "corresponding to DATE=%s\n",
                       conf_instance_get_item_value_ref(conf_instance, "DATE"));

            if (conf_instance_has_item(conf_instance, "DAYS"))
                printf("** ERROR: Could not determine REPORT step "
                       "corresponding to DAYS=%s\n",
                       conf_instance_get_item_value_ref(conf_instance, "DAYS"));
        }
    }
    if (obs_restart_nr < 0)
        util_abort("%s: Failed to look up restart nr correctly \n", __func__);

    return obs_restart_nr;
}

static void obs_vector_resize(obs_vector_type *vector, int new_size) {
    int current_size = vector_get_size(vector->nodes);
    int i;

    for (i = current_size; i < new_size; i++)
        vector_append_ref(vector->nodes, NULL);
}

obs_vector_type *obs_vector_alloc(obs_impl_type obs_type, const char *obs_key,
                                  enkf_config_node_type *config_node,
                                  int num_reports) {
    auto vector = new obs_vector_type;

    UTIL_TYPE_ID_INIT(vector, OBS_VECTOR_TYPE_ID);
    vector->freef = NULL;
    vector->measure = NULL;
    vector->get_obs = NULL;
    vector->user_get = NULL;
    vector->chi2 = NULL;
    vector->update_std_scale = NULL;

    switch (obs_type) {
    case (SUMMARY_OBS):
        vector->freef = summary_obs_free__;
        vector->measure = summary_obs_measure__;
        vector->get_obs = summary_obs_get_observations__;
        vector->user_get = summary_obs_user_get__;
        vector->chi2 = summary_obs_chi2__;
        vector->update_std_scale = summary_obs_update_std_scale__;
        break;
    case (BLOCK_OBS):
        vector->freef = block_obs_free__;
        vector->measure = block_obs_measure__;
        vector->get_obs = block_obs_get_observations__;
        vector->user_get = block_obs_user_get__;
        vector->chi2 = block_obs_chi2__;
        vector->update_std_scale = block_obs_update_std_scale__;
        break;
    case (GEN_OBS):
        vector->freef = gen_obs_free__;
        vector->measure = gen_obs_measure__;
        vector->get_obs = gen_obs_get_observations__;
        vector->user_get = gen_obs_user_get__;
        vector->chi2 = gen_obs_chi2__;
        vector->update_std_scale = gen_obs_update_std_scale__;
        break;
    default:
        util_abort("%s: internal error - obs_type:%d not recognized \n",
                   __func__, obs_type);
    }

    vector->obs_type = obs_type;
    vector->config_node = config_node;
    vector->obs_key = util_alloc_string_copy(obs_key);
    vector->num_active = 0;
    vector->nodes = vector_alloc_new();
    obs_vector_resize(
        vector, num_reports + 1); /* +1 here ?? Ohh  - these +/- problems. */

    return vector;
}

obs_impl_type obs_vector_get_impl_type(const obs_vector_type *obs_vector) {
    return obs_vector->obs_type;
}

/**
   This is the key for the enkf_node which this observation is
   'looking at'. I.e. if this observation is an RFT pressure
   measurement, this function will return "PRESSURE".
*/
const char *obs_vector_get_state_kw(const obs_vector_type *obs_vector) {
    return enkf_config_node_get_key(obs_vector->config_node);
}

const char *obs_vector_get_key(const obs_vector_type *obs_vector) {
    return obs_vector->obs_key;
}

enkf_config_node_type *
obs_vector_get_config_node(const obs_vector_type *obs_vector) {
    return obs_vector->config_node;
}

void obs_vector_free(obs_vector_type *obs_vector) {
    vector_free(obs_vector->nodes);
    free(obs_vector->obs_key);
    delete obs_vector;
}

static void obs_vector_assert_node_type(const obs_vector_type *obs_vector,
                                        const void *node) {
    bool type_OK;
    switch (obs_vector->obs_type) {
    case (SUMMARY_OBS):
        type_OK = summary_obs_is_instance(node);
        break;
    case (BLOCK_OBS):
        type_OK = block_obs_is_instance(node);
        break;
    case (GEN_OBS):
        type_OK = gen_obs_is_instance(node);
        break;
    default:
        util_abort("%s: Error in type check: \n", __func__);
        type_OK = false;
    }
    if (!type_OK)
        util_abort("%s: Type mismatch when trying to add observation node to "
                   "observation vector \n",
                   __func__);
}

void obs_vector_install_node(obs_vector_type *obs_vector, int index,
                             void *node) {
    obs_vector_assert_node_type(obs_vector, node);
    {
        if (vector_iget_const(obs_vector->nodes, index) == NULL) {
            obs_vector->num_active++;
            obs_vector->step_list.push_back(index);
            std::sort(obs_vector->step_list.begin(),
                      obs_vector->step_list.end());
        }

        vector_iset_owned_ref(obs_vector->nodes, index, node,
                              obs_vector->freef);
    }
}

/**
   Observe that @summary_key is the key used to look up the
   corresponding simulated value in the ensemble, and not the
   observation key - the two can be different.
*/
static void obs_vector_add_summary_obs(obs_vector_type *obs_vector,
                                       int obs_index, const char *summary_key,
                                       const char *obs_key, double value,
                                       double std) {
    summary_obs_type *summary_obs =
        summary_obs_alloc(summary_key, obs_key, value, std);
    obs_vector_install_node(obs_vector, obs_index, summary_obs);
}

int obs_vector_get_num_active(const obs_vector_type *vector) {
    return vector->num_active;
}

const std::vector<int> &
obs_vector_get_step_list(const obs_vector_type *vector) {
    return vector->step_list;
}

bool obs_vector_iget_active(const obs_vector_type *vector, int index) {
    /* We accept this ... */
    if (index >= vector_get_size(vector->nodes))
        return false;

    {
        void *obs_data = (void *)vector_iget(vector->nodes, index);
        if (obs_data != NULL)
            return true;
        else
            return false;
    }
}

/**
   Will happily return NULL if index is not active.
*/
void *obs_vector_iget_node(const obs_vector_type *vector, int index) {
    return vector_iget(vector->nodes, index); // CXX_CAST_ERROR
}

void obs_vector_user_get(const obs_vector_type *obs_vector,
                         const char *index_key, int report_step, double *value,
                         double *std, bool *valid) {
    void *obs_node = obs_vector_iget_node(obs_vector, report_step);
    obs_vector->user_get(obs_node, index_key, value, std, valid);
}

/**
  This function returns the next active (i.e. node != NULL) report
  step, starting with 'prev_step + 1'. If no more active steps are
  found, it will return -1.
*/
int obs_vector_get_next_active_step(const obs_vector_type *obs_vector,
                                    int prev_step) {
    if (prev_step >= (vector_get_size(obs_vector->nodes) - 1))
        return -1;
    else {
        int size = vector_get_size(obs_vector->nodes);
        int next_step = prev_step + 1;
        while ((next_step < size) &&
               (obs_vector_iget_node(obs_vector, next_step) == NULL))
            next_step++;

        if (next_step == size)
            return -1; /* No more active steps. */
        else
            return next_step;
    }
}

/**
   All the obs_vector_load_from_XXXX() functions can safely return
   NULL, in which case no observation is added to enkf_obs observation
   hash table.
*/
void obs_vector_load_from_SUMMARY_OBSERVATION(
    obs_vector_type *obs_vector, const conf_instance_type *conf_instance,
    time_map_type *obs_time, ensemble_config_type *ensemble_config) {
    if (!conf_instance_is_of_class(conf_instance, "SUMMARY_OBSERVATION"))
        util_abort("%s: internal error. expected \"SUMMARY_OBSERVATION\" "
                   "instance, got \"%s\".\n",
                   __func__, conf_instance_get_class_name_ref(conf_instance));

    {
        double obs_value =
            conf_instance_get_item_value_double(conf_instance, "VALUE");
        double obs_error =
            conf_instance_get_item_value_double(conf_instance, "ERROR");
        double min_error =
            conf_instance_get_item_value_double(conf_instance, "ERROR_MIN");
        const char *error_mode =
            conf_instance_get_item_value_ref(conf_instance, "ERROR_MODE");
        const char *sum_key =
            conf_instance_get_item_value_ref(conf_instance, "KEY");
        const char *obs_key = conf_instance_get_name_ref(conf_instance);
        int obs_restart_nr = __conf_instance_get_restart_nr(
            conf_instance, obs_key, obs_time, false);

        if (obs_restart_nr == 0) {
            int day, month, year;
            time_t start_time = time_map_iget(obs_time, 0);
            util_set_date_values_utc(start_time, &day, &month, &year);

            fprintf(stderr, "** ERROR: It is unfortunately not possible to use "
                            "summary observations from the\n");
            fprintf(stderr,
                    "          start of the simulation. Problem with "
                    "observation:%s at %02d/%02d/%4d\n",
                    obs_key, day, month, year);
            exit(1);
        }
        {
            if (strcmp(error_mode, "REL") == 0)
                obs_error *= obs_value;
            else if (strcmp(error_mode, "RELMIN") == 0)
                obs_error = util_double_max(min_error, obs_error * obs_value);

            obs_vector_add_summary_obs(obs_vector, obs_restart_nr, sum_key,
                                       obs_key, obs_value, obs_error);
        }
    }
}

obs_vector_type *obs_vector_alloc_from_GENERAL_OBSERVATION(
    const conf_instance_type *conf_instance, time_map_type *obs_time,
    const ensemble_config_type *ensemble_config) {
    if (!conf_instance_is_of_class(conf_instance, "GENERAL_OBSERVATION"))
        util_abort("%s: internal error. expected \"GENERAL_OBSERVATION\" "
                   "instance, got \"%s\".\n",
                   __func__, conf_instance_get_class_name_ref(conf_instance));
    const char *obs_key = conf_instance_get_name_ref(conf_instance);
    const char *state_kw =
        conf_instance_get_item_value_ref(conf_instance, "DATA");
    if (ensemble_config_has_key(ensemble_config, state_kw)) {
        const char *obs_key = conf_instance_get_name_ref(conf_instance);
        int obs_restart_nr = __conf_instance_get_restart_nr(
            conf_instance, obs_key, obs_time, true);
        const char *index_file = NULL;
        const char *index_list = NULL;
        const char *obs_file = NULL;

        if (conf_instance_has_item(conf_instance, "INDEX_FILE"))
            index_file =
                conf_instance_get_item_value_ref(conf_instance, "INDEX_FILE");

        if (conf_instance_has_item(conf_instance, "INDEX_LIST"))
            index_list =
                conf_instance_get_item_value_ref(conf_instance, "INDEX_LIST");

        if (conf_instance_has_item(conf_instance, "OBS_FILE"))
            obs_file =
                conf_instance_get_item_value_ref(conf_instance, "OBS_FILE");

        {
            obs_vector_type *obs_vector = NULL;
            const enkf_config_node_type *config_node =
                ensemble_config_get_node(ensemble_config, state_kw);

            if (enkf_config_node_get_impl_type(config_node) == GEN_DATA) {
                double scalar_error = -1;
                double scalar_value = -1;
                gen_obs_type *gen_obs;
                const gen_data_config_type *config =
                    (const gen_data_config_type *)enkf_config_node_get_ref(
                        config_node);

                if (gen_data_config_has_report_step(config, obs_restart_nr)) {
                    obs_vector = obs_vector_alloc(
                        GEN_OBS, obs_key,
                        ensemble_config_get_node(ensemble_config, state_kw),
                        time_map_get_last_step(obs_time));
                    if (conf_instance_has_item(conf_instance, "VALUE")) {
                        scalar_value = conf_instance_get_item_value_double(
                            conf_instance, "VALUE");
                        scalar_error = conf_instance_get_item_value_double(
                            conf_instance, "ERROR");
                    }

                    /* The config system has ensured that we have either OBS_FILE or (VALUE and ERROR). */
                    gen_obs = gen_obs_alloc(
                        (const gen_data_config_type *)enkf_config_node_get_ref(
                            config_node),
                        obs_key, obs_file, scalar_value, scalar_error,
                        index_file, index_list);
                    obs_vector_install_node(obs_vector, obs_restart_nr,
                                            gen_obs);
                } else
                    fprintf(stderr,
                            "** ERROR: The GEN_DATA node:%s is not configured "
                            "to load from report step:%d - the observation:%s "
                            "will be ignored\n",
                            state_kw, obs_restart_nr, obs_key);
            } else {
                ert_impl_type impl_type =
                    enkf_config_node_get_impl_type(config_node);
                fprintf(stderr,
                        "** ERROR: %s: %s has implementation type:\'%s\' - "
                        "expected:\'%s\' - observation:%s ignored.\n",
                        __func__, state_kw, enkf_types_get_impl_name(impl_type),
                        enkf_types_get_impl_name(GEN_DATA), obs_key);
            }
            return obs_vector;
        }
    } else {
        fprintf(stderr,
                "** Warning the ensemble key:%s does not exist - "
                "observation:%s not added \n",
                state_kw, obs_key);
        return NULL;
    }
}

// Should check the refcase for key - if it is != NULL.

bool obs_vector_load_from_HISTORY_OBSERVATION(
    obs_vector_type *obs_vector, const conf_instance_type *conf_instance,
    time_map_type *obs_time, const history_type *history,
    ensemble_config_type *ensemble_config, double std_cutoff) {

    if (!conf_instance_is_of_class(conf_instance, "HISTORY_OBSERVATION"))
        util_abort("%s: internal error. expected \"HISTORY_OBSERVATION\" "
                   "instance, got \"%s\".\n",
                   __func__, conf_instance_get_class_name_ref(conf_instance));

    {
        bool initOK = false;
        int size, restart_nr;
        double_vector_type *value = double_vector_alloc(0, 0);
        double_vector_type *std = double_vector_alloc(0, 0);
        bool_vector_type *valid = bool_vector_alloc(0, false);

        double error =
            conf_instance_get_item_value_double(conf_instance, "ERROR");
        double error_min =
            conf_instance_get_item_value_double(conf_instance, "ERROR_MIN");
        const char *error_mode =
            conf_instance_get_item_value_ref(conf_instance, "ERROR_MODE");
        const char *sum_key = conf_instance_get_name_ref(conf_instance);

        // Get time series data from history object and allocate
        size = time_map_get_last_step(obs_time) + 1;
        if (history_init_ts(history, sum_key, value, valid)) {
            // Create  the standard deviation vector
            if (strcmp(error_mode, "ABS") == 0) {
                for (restart_nr = 0; restart_nr < size; restart_nr++)
                    double_vector_iset(std, restart_nr, error);
            } else if (strcmp(error_mode, "REL") == 0) {
                for (restart_nr = 0; restart_nr < size; restart_nr++)
                    double_vector_iset(std, restart_nr,
                                       error * std::abs(double_vector_iget(
                                                   value, restart_nr)));
            } else if (strcmp(error_mode, "RELMIN") == 0) {
                for (restart_nr = 0; restart_nr < size; restart_nr++) {
                    double tmp_std = util_double_max(
                        error_min, error * std::abs(double_vector_iget(
                                               value, restart_nr)));
                    double_vector_iset(std, restart_nr, tmp_std);
                }
            } else
                util_abort("%s: Internal error. Unknown error mode \"%s\"\n",
                           __func__, error_mode);

            // Handle SEGMENTs which can be used to customize the observation error. */
            {
                stringlist_type *segment_keys =
                    conf_instance_alloc_list_of_sub_instances_of_class_by_name(
                        conf_instance, "SEGMENT");
                stringlist_sort(segment_keys, NULL);

                int num_segments = stringlist_get_size(segment_keys);

                for (int segment_nr = 0; segment_nr < num_segments;
                     segment_nr++) {
                    const char *segment_name =
                        stringlist_iget(segment_keys, segment_nr);
                    const conf_instance_type *segment_conf =
                        conf_instance_get_sub_instance_ref(conf_instance,
                                                           segment_name);

                    int start =
                        conf_instance_get_item_value_int(segment_conf, "START");
                    int stop =
                        conf_instance_get_item_value_int(segment_conf, "STOP");
                    double error_segment = conf_instance_get_item_value_double(
                        segment_conf, "ERROR");
                    double error_min_segment =
                        conf_instance_get_item_value_double(segment_conf,
                                                            "ERROR_MIN");
                    const char *error_mode_segment =
                        conf_instance_get_item_value_ref(segment_conf,
                                                         "ERROR_MODE");

                    if (start < 0) {
                        printf("%s: WARNING - Segment out of bounds. "
                               "Truncating start of segment to 0.\n",
                               __func__);
                        start = 0;
                    }

                    if (stop >= size) {
                        printf("%s: WARNING - Segment out of bounds. "
                               "Truncating end of segment to %d.\n",
                               __func__, size - 1);
                        stop = size - 1;
                    }

                    if (start > stop) {
                        printf("%s: WARNING - Segment start after stop. "
                               "Truncating end of segment to %d.\n",
                               __func__, start);
                        stop = start;
                    }

                    // Create  the standard deviation vector
                    if (strcmp(error_mode_segment, "ABS") == 0) {
                        for (restart_nr = start; restart_nr <= stop;
                             restart_nr++)
                            double_vector_iset(std, restart_nr, error_segment);
                    } else if (strcmp(error_mode_segment, "REL") == 0) {
                        for (restart_nr = start; restart_nr <= stop;
                             restart_nr++)
                            double_vector_iset(std, restart_nr,
                                               error_segment *
                                                   std::abs(double_vector_iget(
                                                       value, restart_nr)));
                    } else if (strcmp(error_mode_segment, "RELMIN") == 0) {
                        for (restart_nr = start; restart_nr <= stop;
                             restart_nr++) {
                            double tmp_std = util_double_max(
                                error_min_segment,
                                error_segment * std::abs(double_vector_iget(
                                                    value, restart_nr)));
                            double_vector_iset(std, restart_nr, tmp_std);
                        }
                    } else
                        util_abort(
                            "%s: Internal error. Unknown error mode \"%s\"\n",
                            __func__, error_mode);
                }
                stringlist_free(segment_keys);
            }

            /*
        This is where the summary observations are finally added.
      */
            for (restart_nr = 0; restart_nr < size; restart_nr++) {
                if (bool_vector_safe_iget(valid, restart_nr)) {
                    if (double_vector_iget(std, restart_nr) > std_cutoff) {
                        obs_vector_add_summary_obs(
                            obs_vector, restart_nr, sum_key, sum_key,
                            double_vector_iget(value, restart_nr),
                            double_vector_iget(std, restart_nr));
                    } else
                        fprintf(stderr,
                                "** Warning: to small observation error in "
                                "observation %s:%d - ignored. \n",
                                sum_key, restart_nr);
                }
            }
            initOK = true;
        }
        double_vector_free(std);
        double_vector_free(value);
        bool_vector_free(valid);
        return initOK;
    }
}

static const char *__summary_kw(const char *field_name) {
    if (strcmp(field_name, "PRESSURE") == 0)
        return "BPR";
    else if (strcmp(field_name, "SWAT") == 0)
        return "BSWAT";
    else if (strcmp(field_name, "SGAS") == 0)
        return "BSGAS";
    else {
        util_abort("%s: sorry - could not \'translate\' field:%s to block "
                   "summary variable\n",
                   __func__, field_name);
        return NULL;
    }
}

obs_vector_type *obs_vector_alloc_from_BLOCK_OBSERVATION(
    const conf_instance_type *conf_instance, const ecl_grid_type *grid,
    time_map_type *obs_time, const ecl_sum_type *refcase,
    ensemble_config_type *ensemble_config) {

    if (!conf_instance_is_of_class(conf_instance, "BLOCK_OBSERVATION"))
        util_abort("%s: internal error. expected \"BLOCK_OBSERVATION\" "
                   "instance, got \"%s\".\n",
                   __func__, conf_instance_get_class_name_ref(conf_instance));

    block_obs_source_type source_type = SOURCE_SUMMARY;
    const char *obs_label = conf_instance_get_name_ref(conf_instance);
    const char *source_string =
        conf_instance_get_item_value_ref(conf_instance, "SOURCE");
    const char *field_name =
        conf_instance_get_item_value_ref(conf_instance, "FIELD");
    const char *sum_kw = NULL;
    bool OK = true;

    if (strcmp(source_string, "FIELD") == 0) {
        source_type = SOURCE_FIELD;
        if (!ensemble_config_has_key(ensemble_config, field_name)) {
            OK = false;
            fprintf(stderr,
                    "** Warning the ensemble key:%s does not exist - "
                    "observation:%s not added \n",
                    field_name, obs_label);
        }
    } else if (strcmp(source_string, "SUMMARY") == 0) {
        source_type = SOURCE_SUMMARY;
        sum_kw = __summary_kw(field_name);
    } else
        util_abort("%s: internal error \n", __func__);

    if (OK) {
        obs_vector_type *obs_vector = NULL;
        int size = time_map_get_last_step(obs_time);
        int obs_restart_nr;

        stringlist_type *summary_keys = stringlist_alloc_new();
        stringlist_type *obs_pt_keys =
            conf_instance_alloc_list_of_sub_instances_of_class_by_name(
                conf_instance, "OBS");
        int num_obs_pts = stringlist_get_size(obs_pt_keys);

        double *obs_value =
            (double *)util_calloc(num_obs_pts, sizeof *obs_value);
        double *obs_std = (double *)util_calloc(num_obs_pts, sizeof *obs_std);
        int *obs_i = (int *)util_calloc(num_obs_pts, sizeof *obs_i);
        int *obs_j = (int *)util_calloc(num_obs_pts, sizeof *obs_j);
        int *obs_k = (int *)util_calloc(num_obs_pts, sizeof *obs_k);

        obs_restart_nr = __conf_instance_get_restart_nr(
            conf_instance, obs_label, obs_time, false);

        /* Build the observation. */
        for (int obs_pt_nr = 0; obs_pt_nr < num_obs_pts; obs_pt_nr++) {
            const char *obs_key = stringlist_iget(obs_pt_keys, obs_pt_nr);
            const conf_instance_type *obs_instance =
                conf_instance_get_sub_instance_ref(conf_instance, obs_key);
            const char *error_mode =
                conf_instance_get_item_value_ref(obs_instance, "ERROR_MODE");
            double error =
                conf_instance_get_item_value_double(obs_instance, "ERROR");
            double value =
                conf_instance_get_item_value_double(obs_instance, "VALUE");
            double min_error =
                conf_instance_get_item_value_double(obs_instance, "ERROR_MIN");

            if (strcmp(error_mode, "REL") == 0)
                error *= value;
            else if (strcmp(error_mode, "RELMIN") == 0)
                error = util_double_max(error * value, min_error);

            obs_value[obs_pt_nr] = value;
            obs_std[obs_pt_nr] = error;

            // The input values i,j,k come from the user, and are offset 1.
            // They are immediately shifted with -1 to become C-based offset zero.
            obs_i[obs_pt_nr] =
                conf_instance_get_item_value_int(obs_instance, "I") - 1;
            obs_j[obs_pt_nr] =
                conf_instance_get_item_value_int(obs_instance, "J") - 1;
            obs_k[obs_pt_nr] =
                conf_instance_get_item_value_int(obs_instance, "K") - 1;

            if (source_type == SOURCE_SUMMARY) {
                char *summary_key = smspec_alloc_block_ijk_key(
                    SUMMARY_KEY_JOIN_STRING, sum_kw, obs_i[obs_pt_nr] + 1,
                    obs_j[obs_pt_nr] + 1, obs_k[obs_pt_nr] + 1);

                stringlist_append_copy(summary_keys, summary_key);
                free(summary_key);
            }
        }

        if (source_type == SOURCE_FIELD) {
            const enkf_config_node_type *config_node =
                ensemble_config_get_node(ensemble_config, field_name);
            const field_config_type *field_config =
                (const field_config_type *)enkf_config_node_get_ref(
                    config_node);
            block_obs_type *block_obs = block_obs_alloc_complete(
                obs_label, source_type, NULL, field_config, grid, num_obs_pts,
                obs_i, obs_j, obs_k, obs_value, obs_std);

            if (block_obs != NULL) {
                obs_vector = obs_vector_alloc(
                    BLOCK_OBS, obs_label,
                    ensemble_config_get_node(ensemble_config, field_name),
                    size);
                obs_vector_install_node(obs_vector, obs_restart_nr, block_obs);
            }
        } else if (source_type == SOURCE_SUMMARY) {
            OK = true;
            if (refcase != NULL) {
                for (int i = 0; i < stringlist_get_size(summary_keys); i++) {
                    const char *sum_key = stringlist_iget(summary_keys, i);
                    if (!ecl_sum_has_key(refcase, sum_key)) {
                        fprintf(stderr,
                                "** Warning missing summary %s for cell: "
                                "(%d,%d,%d) in refcase - make sure that \"BPR  "
                                "%d  %d  %d\" is included in ECLIPSE summary "
                                "specification \n",
                                sum_key, obs_i[i] + 1, obs_j[i] + 1,
                                obs_k[i] + 1, obs_i[i] + 1, obs_j[i] + 1,
                                obs_k[i] + 1);
                        //OK = false;
                    }
                }
            }
            if (OK) {
                // We can create the container node and add the summary nodes.
                enkf_config_node_type *container_config =
                    ensemble_config_add_container(ensemble_config, NULL);

                for (int i = 0; i < stringlist_get_size(summary_keys); i++) {
                    const char *sum_key = stringlist_iget(summary_keys, i);
                    enkf_config_node_type *child_node =
                        ensemble_config_add_summary_observation(
                            ensemble_config, sum_key, LOAD_FAIL_WARN);
                    enkf_config_node_update_container(container_config,
                                                      child_node);
                }

                {
                    block_obs_type *block_obs = block_obs_alloc_complete(
                        obs_label, source_type, summary_keys,
                        enkf_config_node_get_ref(
                            container_config), // CXX_CAST_ERROR
                        grid, num_obs_pts, obs_i, obs_j, obs_k, obs_value,
                        obs_std);
                    if (block_obs != NULL) {
                        obs_vector = obs_vector_alloc(BLOCK_OBS, obs_label,
                                                      container_config, size);
                        obs_vector_install_node(obs_vector, obs_restart_nr,
                                                block_obs);
                    }
                }
            }
        } else
            util_abort("%s: invalid source value \n", __func__);

        free(obs_value);
        free(obs_std);
        free(obs_i);
        free(obs_j);
        free(obs_k);
        stringlist_free(obs_pt_keys);
        stringlist_free(summary_keys);

        return obs_vector;
    } else {
        fprintf(stderr,
                "** Warning the ensemble key:%s does not exist - "
                "observation:%s not added \n",
                field_name, obs_label);
        return NULL;
    }
}

void obs_vector_iget_observations(const obs_vector_type *obs_vector,
                                  int report_step, obs_data_type *obs_data,
                                  enkf_fs_type *fs) {
    void *obs_node = (void *)vector_iget(obs_vector->nodes, report_step);
    if (obs_node != NULL)
        obs_vector->get_obs(obs_node, obs_data, fs, report_step);
}

void obs_vector_measure(const obs_vector_type *obs_vector, enkf_fs_type *fs,
                        int report_step,
                        const std::vector<int> &ens_active_list,
                        meas_data_type *meas_data) {

    void *obs_node = (void *)vector_iget(obs_vector->nodes, report_step);
    if (obs_node != NULL) {
        enkf_node_type *enkf_node =
            enkf_node_deep_alloc(obs_vector->config_node);

        node_id_type node_id = {.report_step = report_step, .iens = 0};

        int vec_size = ens_active_list.size();
        for (int active_iens_index = 0; active_iens_index < vec_size;
             active_iens_index++) {
            node_id.iens = ens_active_list[active_iens_index];

            enkf_node_load(enkf_node, fs, node_id);
            obs_vector->measure(obs_node, enkf_node_value_ptr(enkf_node),
                                node_id, meas_data);
        }

        enkf_node_free(enkf_node);
    }
}

static bool
obs_vector_has_data_at_report_step(const obs_vector_type *obs_vector,
                                   const bool_vector_type *active_mask,
                                   enkf_fs_type *fs, int report_step) {
    void *obs_node = (void *)vector_iget(obs_vector->nodes, report_step);
    if (obs_node) {
        node_id_type node_id = {.report_step = report_step};
        for (int iens = 0; iens < bool_vector_size(active_mask); iens++) {
            if (bool_vector_iget(active_mask, iens)) {
                node_id.iens = iens;
                if (!enkf_config_node_has_node(obs_vector->config_node, fs,
                                               node_id))
                    return false;
            }
        }
    }

    /*
     Will return true unconditionally if we do not have observation data at this report step;
     or alternatively if the active_mask is all false.
  */
    return true;
}

/**
  The has_vector_data() function will only check that we have a vector
  stored, and not the actual length of the vector. This means we can
  be fooled if the stored vector is shorter than what the observation
  requires.

  Should ideally check that the vector is long enough, but that
  requires changes in the enkf_node api for vector storage.
*/
static bool obs_vector_has_vector_data(const obs_vector_type *obs_vector,
                                       const bool_vector_type *active_mask,
                                       enkf_fs_type *fs) {
    int vec_size = bool_vector_size(active_mask);

    for (int iens = 0; iens < vec_size; iens++) {
        const enkf_config_node_type *data_config = obs_vector->config_node;
        if (bool_vector_iget(active_mask, iens)) {
            if (!enkf_config_node_has_vector(data_config, fs, iens)) {
                return false;
            }
        }
    }

    return true;
}

bool obs_vector_has_data(const obs_vector_type *obs_vector,
                         const bool_vector_type *active_mask,
                         enkf_fs_type *fs) {
    const enkf_config_node_type *data_config = obs_vector->config_node;
    if (enkf_config_node_vector_storage(data_config))
        return obs_vector_has_vector_data(obs_vector, active_mask, fs);

    int vec_size = vector_get_size(obs_vector->nodes);
    for (int report_step = 0; report_step < vec_size; report_step++) {
        if (!obs_vector_has_data_at_report_step(obs_vector, active_mask, fs,
                                                report_step))
            return false;
    }
    return true;
}

/**
   This is the lowest level function:

   * It is checked that the obs_vector is active for the actual report
     step; if it is not active 0.0 is returned without any further
     ado.

   * It is assumed the enkf_node_instance contains valid data for this
     report_step. This is not checked in this function, and is the
     responsability of the calling scope.

   * The underlying chi2 function will do a type-check of node - and
     fail hard if it is not correct.

*/
static double obs_vector_chi2__(const obs_vector_type *obs_vector,
                                int report_step, const enkf_node_type *node,
                                node_id_type node_id) {
    void *obs_node = (void *)vector_iget(obs_vector->nodes, report_step);

    if (obs_node)
        return obs_vector->chi2(obs_node, enkf_node_value_ptr(node), node_id);
    else
        return 0.0; /* Observation not active for this report step. */
}

/**
   This function will evaluate the chi2 for the ensemble members
   [iens1,iens2) and report steps [step1,step2).

   Observe that the chi2 pointer is assumed to be allocated for the
   complete ensemble, altough this function only operates on part of
   it.

   This will not work for container observations .....
*/
void obs_vector_ensemble_chi2(const obs_vector_type *obs_vector,
                              enkf_fs_type *fs, bool_vector_type *valid,
                              int step1, int step2, int iens1, int iens2,
                              double **chi2) {

    int step;
    enkf_node_type *enkf_node = enkf_node_alloc(obs_vector->config_node);
    node_id_type node_id;
    for (step = step1; step <= step2; step++) {
        int iens;
        node_id.report_step = step;
        {
            void *obs_node = (void *)vector_iget(obs_vector->nodes, step);

            if (obs_node == NULL) {
                for (iens = iens1; iens < iens2; iens++)
                    chi2[step][iens] = 0;
            } else {
                for (iens = iens1; iens < iens2; iens++) {
                    node_id.iens = iens;
                    if (enkf_node_try_load(enkf_node, fs, node_id))
                        chi2[step][iens] = obs_vector_chi2__(
                            obs_vector, step, enkf_node, node_id);
                    else {
                        chi2[step][iens] = 0;
                        // Missing data - this member will be marked as invalid in the misfit calculations.
                        bool_vector_iset(valid, iens, false);
                    }
                }
            }
        }
    }
    enkf_node_free(enkf_node);
}

/**
   This function will evaluate the total chi2 for one ensemble member
   (i.e. sum over report steps).
*/
double obs_vector_total_chi2(const obs_vector_type *obs_vector,
                             enkf_fs_type *fs, int iens) {
    double sum_chi2 = 0;
    enkf_node_type *enkf_node = enkf_node_deep_alloc(obs_vector->config_node);
    node_id_type node_id = {.report_step = 0, .iens = iens};

    int vec_size = vector_get_size(obs_vector->nodes);
    for (int report_step = 0; report_step < vec_size; report_step++) {
        if (vector_iget(obs_vector->nodes, report_step) != NULL) {
            node_id.report_step = report_step;

            if (enkf_node_try_load(enkf_node, fs, node_id))
                sum_chi2 += obs_vector_chi2__(obs_vector, report_step,
                                              enkf_node, node_id);
        }
    }
    enkf_node_free(enkf_node);
    return sum_chi2;
}

const char *obs_vector_get_obs_key(const obs_vector_type *obs_vector) {
    return obs_vector->obs_key;
}

VOID_FREE(obs_vector)

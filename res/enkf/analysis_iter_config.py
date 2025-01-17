#  Copyright (C) 2013  Equinor ASA, Norway.
#
#  The file 'analysis_iter_config.py' is part of ERT - Ensemble based Reservoir Tool.
#
#  ERT is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  ERT is distributed in the hope that it will be useful, but WITHOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.
#
#  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
#  for more details.
from cwrap import BaseCClass

from res import ResPrototype
from res.enkf.config_keys import ConfigKeys


class AnalysisIterConfig(BaseCClass):
    TYPE_NAME = "analysis_iter_config"

    _alloc = ResPrototype("void* analysis_iter_config_alloc( )", bind=False)
    _alloc_full = ResPrototype(
        "void* analysis_iter_config_alloc_full( char*, int, int )", bind=False
    )
    _free = ResPrototype("void  analysis_iter_config_free( analysis_iter_config )")
    _set_num_iterations = ResPrototype(
        "void  analysis_iter_config_set_num_iterations(analysis_iter_config, int)"
    )
    _get_num_iterations = ResPrototype(
        "int   analysis_iter_config_get_num_iterations(analysis_iter_config)"
    )
    _get_num_retries = ResPrototype(
        "int   analysis_iter_config_get_num_retries_per_iteration(analysis_iter_config)"
    )
    _num_iterations_set = ResPrototype(
        "bool  analysis_iter_config_num_iterations_set(analysis_iter_config)"
    )
    _set_case_fmt = ResPrototype(
        "void  analysis_iter_config_set_case_fmt( analysis_iter_config , char* )"
    )
    _get_case_fmt = ResPrototype(
        "char* analysis_iter_config_get_case_fmt( analysis_iter_config)"
    )
    _case_fmt_set = ResPrototype(
        "bool  analysis_iter_config_case_fmt_set(analysis_iter_config)"
    )

    def __init__(self, config_dict=None):

        if config_dict is None:
            c_ptr = self._alloc()
            if c_ptr:
                super().__init__(c_ptr)
            else:
                raise ValueError("Failed to construct AnalysisIterConfig instance.")
        else:
            c_ptr = self._alloc_full(
                config_dict[ConfigKeys.ITER_CASE],
                config_dict[ConfigKeys.ITER_COUNT],
                config_dict[ConfigKeys.ITER_RETRY_COUNT],
            )
            if c_ptr:
                super().__init__(c_ptr)
            else:
                raise ValueError(
                    "Failed to construct AnalysisIterConfig instance for dictionary."
                )

    def getNumIterations(self):
        """@rtype: int"""
        return self._get_num_iterations()

    def __len__(self):
        """Returns number of iterations."""
        return self.getNumIterations()

    def setNumIterations(self, num_iterations):
        self._set_num_iterations(num_iterations)

    def numIterationsSet(self):
        return self._num_iterations_set()

    def getNumRetries(self):
        """@rtype: int"""
        return self._get_num_retries()

    def getCaseFormat(self):
        """@rtype: str"""
        return self._get_case_fmt()

    def setCaseFormat(self, case_fmt):
        self._set_case_fmt(case_fmt)

    def caseFormatSet(self):
        return self._case_fmt_set()

    def free(self):
        self._free()

    def _short_case_fmt(self, maxlen=10):
        fmt = self.getCaseFormat()
        if len(fmt) <= maxlen:
            return fmt
        return fmt[: maxlen - 2] + ".."

    def __repr__(self):
        cfs = "format = False"
        if self.caseFormatSet():
            cfs = "format = True"
        fmt = self._short_case_fmt()
        ret = "AnalysisIterConfig(iterations = %d, retries = %d, fmt = %s, %s) at 0x%x"
        its = len(self)
        rets = self.getNumRetries()
        return ret % (its, rets, fmt, cfs, self._address())

    def __ne__(self, other):
        return not self == other

    def __eq__(self, other):
        if self.getCaseFormat() != other.getCaseFormat():
            return False

        if self.getNumIterations() != other.getNumIterations():
            return False

        if self.getNumRetries() != other.getNumRetries():
            return False

        return True

#  Copyright (C) 2013  Equinor ASA, Norway.
#
#  The file 'ert_impl_type_enum.py' is part of ERT - Ensemble based Reservoir Tool.
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
from cwrap import BaseCEnum


class RealizationStateEnum(BaseCEnum):
    TYPE_NAME = "realisation_state_enum"
    STATE_UNDEFINED = 1
    STATE_INITIALIZED = 2
    STATE_HAS_DATA = 4
    STATE_LOAD_FAILURE = 8
    STATE_PARENT_FAILURE = 16


RealizationStateEnum.addEnum("STATE_UNDEFINED", 1)
RealizationStateEnum.addEnum("STATE_INITIALIZED", 2)
RealizationStateEnum.addEnum("STATE_HAS_DATA", 4)
RealizationStateEnum.addEnum("STATE_LOAD_FAILURE", 8)
RealizationStateEnum.addEnum("STATE_PARENT_FAILURE", 16)

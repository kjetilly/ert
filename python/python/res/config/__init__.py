#  Copyright (C) 2013  Statoil ASA, Norway.
#
#  The file '__init__.py' is part of ERT - Ensemble based Reservoir Tool.
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
from cwrap import Prototype
import ecl
import ecl.util

class ConfigPrototype(Prototype):
    lib = ecl.load("libconfig")

    def __init__(self, prototype, bind=True):
        super(ConfigPrototype, self).__init__(ConfigPrototype.lib, prototype, bind=bind)

from .unrecognized_enum import UnrecognizedEnum
from .content_type_enum import ContentTypeEnum
from .config_error import ConfigError
from .schema_item import SchemaItem
from .config_content import ConfigContent , ContentItem, ContentNode
from .config_parser import ConfigParser
from .config_settings import ConfigSettings

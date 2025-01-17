#  Copyright (C) 2011  Equinor ASA, Norway.
#
#  The file 'job.py' is part of ERT - Ensemble based Reservoir Tool.
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


import datetime
import time

from cwrap import BaseCClass

from .job_status_type_enum import JobStatusType

# This class and the interplay between this class and the Driver and
# JobQueue classes is quite fragile; in particular the Job class
# internalizes a void * pointer to the completely driver specific job
# information - this is way too low level.


class Job(BaseCClass):
    TYPE_NAME = "job"

    def __init__(self, c_ptr, driver):
        self.driver = driver
        self.submit_time = datetime.datetime.now()
        super().__init__(c_ptr)

    def free(self):
        pass

    def block(self):
        while True:
            status = self.status()
            if status in (JobStatusType.JOB_QUEUE_DONE, JobStatusType.JOB_QUEUE_EXIT):
                break
            time.sleep(1)

    def kill(self):
        self.driver.kill_job(self)

    @property
    def run_time(self):
        td = datetime.datetime.now() - self.submit_time
        return td.seconds + td.days * 24 * 3600

    @property
    def status(self):
        st = self.driver.get_status(self)
        return st

    @property
    def running(self):
        status = self.driver.get_status(self)
        return status == JobStatusType.JOB_QUEUE_RUNNING

    @property
    def pending(self):
        status = self.driver.get_status(self)
        return status == JobStatusType.JOB_QUEUE_PENDING

    @property
    def complete(self):
        status = self.driver.get_status(self)
        return status in (JobStatusType.JOB_QUEUE_DONE, JobStatusType.JOB_QUEUE_EXIT)

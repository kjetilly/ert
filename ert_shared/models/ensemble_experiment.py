import logging
import concurrent
import asyncio
from ert_shared.models.base_run_model import ErtRunError
from res.enkf.enkf_main import EnKFMain, QueueConfig
from res.enkf.enums import HookRuntime
from res.enkf import ErtRunContext, EnkfSimulationRunner
import uuid
from ert_shared.models import BaseRunModel
from ert_shared.ensemble_evaluator.config import EvaluatorServerConfig
from typing import Dict, Any
from ert.ensemble_evaluator import identifiers

from cloudevents.http import CloudEvent


logger = logging.getLogger("ert.experiment_server")


class EnsembleExperiment(BaseRunModel):
    def __init__(
        self,
        simulation_arguments: Dict[str, Any],
        ert: EnKFMain,
        queue_config: QueueConfig,
    ):
        super().__init__(simulation_arguments, ert, queue_config)

    async def run(self, evaluator_server_config: EvaluatorServerConfig) -> None:

        # Send EXPERIMENT_STARTED
        logger.debug("starting ensemble experiment")
        await self.dispatch(
            CloudEvent(
                {
                    "type": identifiers.EVTYPE_EXPERIMENT_STARTED,
                    "source": f"/ert/experiment/{self.id_}",
                    "id": str(uuid.uuid1()),
                }
            ),
            0,
        )

        loop = asyncio.get_running_loop()

        with concurrent.futures.ThreadPoolExecutor() as pool:
            run_context = await loop.run_in_executor(pool, self.create_context)

            # Create runpaths
            logger.debug("creating runpaths")
            await loop.run_in_executor(
                pool,
                self.ert().getEnkfSimulationRunner().createRunPath,
                run_context,
            )

            ensemble_id = await loop.run_in_executor(pool, self._post_ensemble_data)

            # Send HOOK_STARTED
            await self.dispatch(
                CloudEvent(
                    {
                        "type": identifiers.EVTYPE_EXPERIMENT_HOOK_STARTED,
                        "source": f"/ert/experiment/{self.id_}",
                        "id": str(uuid.uuid1()),
                    },
                    {
                        "name": "PRE_SIMULATION",
                    },
                ),
                run_context.get_iter(),
            )

            # Run PRE_SIMULATION
            logger.debug("pre-sim hooks")
            await loop.run_in_executor(
                pool,
                EnkfSimulationRunner.runWorkflows,
                HookRuntime.PRE_SIMULATION,
                self.ert(),
            )

            # Send HOOK_ENDED
            await self.dispatch(
                CloudEvent(
                    {
                        "type": identifiers.EVTYPE_EXPERIMENT_HOOK_ENDED,
                        "source": f"/ert/experiment/{self.id_}",
                        "id": str(uuid.uuid1()),
                    },
                    {
                        "name": "PRE_SIMULATION",
                    },
                ),
                run_context.get_iter(),
            )

            # Evaluate
            logger.debug("evaluating")
            await self._evaluate(run_context, evaluator_server_config)

            num_successful_realizations = self._state_machine.successful_realizations(
                run_context.get_iter()
            )

            num_successful_realizations += self._simulation_arguments.get(
                "prev_successful_realizations", 0
            )
            try:
                self.checkHaveSufficientRealizations(num_successful_realizations)
            except ErtRunError as e:

                # Send EXPERIMENT_FAILED
                await self.dispatch(
                    CloudEvent(
                        {
                            "type": identifiers.EVTYPE_EXPERIMENT_FAILED,
                            "source": f"/ert/experiment/{self.id_}",
                            "id": str(uuid.uuid1()),
                        },
                        {
                            "error": str(e),
                        },
                    ),
                    run_context.get_iter(),
                )
                return

            # Send HOOK_STARTED
            await self.dispatch(
                CloudEvent(
                    {
                        "type": identifiers.EVTYPE_EXPERIMENT_HOOK_STARTED,
                        "source": f"/ert/experiment/{self.id_}",
                        "id": str(uuid.uuid1()),
                    },
                    {
                        "name": "POST_SIMULATION",
                    },
                ),
                run_context.get_iter(),
            )

            # Run POST_SIMULATION hook
            await loop.run_in_executor(
                pool,
                EnkfSimulationRunner.runWorkflows,
                HookRuntime.POST_SIMULATION,
                self.ert(),
            )

            # Send HOOK_ENDED
            await self.dispatch(
                CloudEvent(
                    {
                        "type": identifiers.EVTYPE_EXPERIMENT_HOOK_ENDED,
                        "source": f"/ert/experiment/{self.id_}",
                        "id": str(uuid.uuid1()),
                    },
                    {
                        "name": "POST_SIMULATION",
                    },
                ),
                run_context.get_iter(),
            )

            # Push simulation results to storage
            await loop.run_in_executor(
                pool,
                self._post_ensemble_results,
                ensemble_id,
            )

        # Send EXPERIMENT_COMPLETED
        await self.dispatch(
            CloudEvent(
                {
                    "type": identifiers.EVTYPE_EXPERIMENT_SUCCEEDED,
                    "source": f"/ert/experiment/{self.id_}",
                    "id": str(uuid.uuid1()),
                },
            ),
            run_context.get_iter(),
        )

    def runSimulations__(
        self,
        run_msg: str,
        evaluator_server_config: EvaluatorServerConfig,
    ) -> ErtRunContext:

        run_context = self.create_context()

        self.setPhase(0, "Running simulations...", indeterminate=False)

        self.setPhaseName("Pre processing...", indeterminate=True)
        self.ert().getEnkfSimulationRunner().createRunPath(run_context)

        # Push ensemble, parameters, observations to new storage
        ensemble_id = self._post_ensemble_data()

        EnkfSimulationRunner.runWorkflows(HookRuntime.PRE_SIMULATION, self.ert())

        self.setPhaseName(run_msg, indeterminate=False)

        num_successful_realizations = self.run_ensemble_evaluator(
            run_context, evaluator_server_config
        )

        num_successful_realizations += self._simulation_arguments.get(
            "prev_successful_realizations", 0
        )
        self.checkHaveSufficientRealizations(num_successful_realizations)

        self.setPhaseName("Post processing...", indeterminate=True)
        EnkfSimulationRunner.runWorkflows(HookRuntime.POST_SIMULATION, self.ert())

        # Push simulation results to storage
        self._post_ensemble_results(ensemble_id)

        self.setPhase(1, "Simulations completed.")  # done...

        return run_context

    def runSimulations(
        self, evaluator_server_config: EvaluatorServerConfig
    ) -> ErtRunContext:
        return self.runSimulations__(
            "Running ensemble experiment...", evaluator_server_config
        )

    def create_context(self) -> ErtRunContext:
        fs_manager = self.ert().getEnkfFsManager()
        result_fs = fs_manager.getCurrentFileSystem()

        model_config = self.ert().getModelConfig()
        runpath_fmt = model_config.getRunpathFormat()
        jobname_fmt = model_config.getJobnameFormat()
        subst_list = self.ert().getDataKW()
        itr = self._simulation_arguments.get("iter_num", 0)
        mask = self._simulation_arguments["active_realizations"]

        run_context = ErtRunContext.ensemble_experiment(
            result_fs, mask, runpath_fmt, jobname_fmt, subst_list, itr
        )

        self._run_context = run_context
        self._last_run_iteration = run_context.get_iter()
        return run_context

    @classmethod
    def name(cls) -> str:
        return "Ensemble experiment"

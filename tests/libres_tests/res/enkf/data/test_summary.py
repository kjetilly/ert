from libres_utils import ResTest

from res.enkf.config import SummaryConfig
from res.enkf.data.summary import Summary


class SummaryTest(ResTest):
    # pylint: disable=pointless-statement
    def test_create(self):
        config = SummaryConfig("WWCT:OP_5")
        summary = Summary(config)
        self.assertEqual(len(summary), 0)

        with self.assertRaises(IndexError):
            summary[100]

        summary[0] = 75
        self.assertEqual(summary[0], 75)

        summary[10] = 100
        self.assertEqual(summary[10], 100)

        with self.assertRaises(ValueError):
            summary[5]

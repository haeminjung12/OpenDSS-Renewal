import unittest
from pathlib import Path

from .phase3_relaunch2 import PARENTS, config_for


class Relaunch2Tests(unittest.TestCase):
    def test_parents_use_partial_mean_existing(self):
        for condition in PARENTS:
            config = config_for(condition, 1729, Path("app/runtime/models/squeezenet_final_new_condition.onnx").resolve())
            self.assertEqual(config["classifier_initialization"], "partial_mean_existing")
            self.assertEqual(config["classifier_output"], "signed_logits")
            self.assertEqual([stage["learning_rate"] for stage in config["stages"]], [1e-4, 1e-5])

    def test_parent_modes_are_preregistered(self):
        self.assertEqual([item["name"] for item in PARENTS], ["balanced_sampler_only", "effective_cap10"])
        self.assertEqual(PARENTS[0]["imbalance"]["mode"], "balanced_sampler")
        self.assertEqual(PARENTS[1]["imbalance"]["cap"], 10.0)


if __name__ == "__main__": unittest.main()

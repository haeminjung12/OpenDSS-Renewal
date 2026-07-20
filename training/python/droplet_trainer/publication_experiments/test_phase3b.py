import json, unittest

from droplet_trainer.train import _sampler_weights, _validate_training_config
from .phase3b import ALPHAS, condition, eligible, frozen_plan
from .phase3_relaunch2 import config_for


class BoundaryTuningTests(unittest.TestCase):
    def test_endpoints_and_deterministic_weights(self):
        items = [{"class_id": "0"}, {"class_id": "0"}, {"class_id": "1"}]
        self.assertEqual(_sampler_weights(items, 0), [1, 1, 1])
        self.assertEqual(_sampler_weights(items, 1), [.5, .5, 1])
        self.assertEqual(_sampler_weights(items, .75), _sampler_weights(items, .75))

    def test_alpha_validation_and_serialization(self):
        for alpha in ALPHAS:
            cfg = config_for(condition(alpha), 1729, __import__('pathlib').Path('app/runtime/models/squeezenet_final_new_condition.onnx').resolve())
            cfg["split_mode"] = "auto"
            _validate_training_config(cfg)
            self.assertEqual(json.loads(json.dumps(cfg))["imbalance"]["sampler_alpha"], alpha)
        with self.assertRaises(Exception):
            _sampler_weights([{"class_id": "0"}], 1.01)

    def test_plan_order_budget_and_resume_identity(self):
        plan = frozen_plan()
        self.assertEqual([row["imbalance"]["sampler_alpha"] for row in plan["conditions"]], list(ALPHAS))
        self.assertEqual(plan["maximum_primary_runs"], 6); self.assertEqual(plan["maximum_total_runs"], 8)
        self.assertEqual(len({row["name"] for row in plan["conditions"]}), 4)

    def test_pruning_eligibility(self):
        row = {"class_metrics": [{"recall": .9}, {"recall": .61}, {"recall": .51}],
               "diagnostics": {"failure_reasons": []}, "metric_recomputation": {"agreement": True}}
        self.assertTrue(eligible(row)); row["class_metrics"][2]["recall"] = .49; self.assertFalse(eligible(row))


if __name__ == "__main__": unittest.main()

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import os
import platform
import statistics
import time
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import numpy as np

from ..schema import ClassSchema
from ..train import (
    ManifestImageDataset, _build_model, _build_transforms, _export_onnx,
    _freeze_for_stage, _logit_diagnostics, _make_cuda_amp_scaler,
    _make_optimizer, _parameter_delta_l2, _run_epoch, _seed_everything,
    _trainable_parameter_summary,
)
from .architecture_comparison import weight_provenance
from .latency_benchmark import latency_statistics
from .screen import sha256_file

ARCHITECTURES = ("mobilenet_v3_small", "efficientnet_b0")
EXPECTED_COUNTS = {"0": 3142, "1": 1055, "2": 114}


def _canonical_hash(value: Any) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def derive_schedule(stage_epochs: dict[str, list[list[int]]]) -> dict[str, list[int]]:
    """Conservative deterministic rule: upper-rounded median per stage."""
    return {arch: [int(math.ceil(statistics.median(values))) for values in zip(*folds)] for arch, folds in stage_epochs.items()}


def build_records(derived_path: Path) -> list[dict[str, str]]:
    derived = json.loads(derived_path.read_text(encoding="utf-8"))
    records = []
    for row in derived["records"]:
        source = (Path(row["source_root"]) / row["source_path"]).resolve()
        records.append({"record_id": row["record_id"], "source_path": str(source), "class_id": str(row["class_id"]), "sha256": row["sha256"], "role": "train"})
    return records


def make_contract(records: list[dict[str, str]], derived_path: Path, fold_path: Path) -> dict[str, Any]:
    items = {row["record_id"]: {key: row[key] for key in ("source_path", "class_id", "sha256", "role")} for row in records}
    return {"mode": "all_data_final_fit", "authorized": True, "classes": ["0", "1", "2"], "count": len(records),
            "class_counts": dict(Counter(row["class_id"] for row in records)), "identities_sha256": hashlib.sha256("\n".join(sorted(items)).encode()).hexdigest(),
            "items_sha256": _canonical_hash(items), "items": items, "derived_manifest": str(derived_path), "derived_manifest_sha256": sha256_file(derived_path),
            "fold_manifest": str(fold_path), "fold_manifest_sha256": sha256_file(fold_path)}


def validate_contract(records: list[dict[str, str]], contract: dict[str, Any], verify_content: bool = True) -> dict[str, Any]:
    if contract.get("mode") != "all_data_final_fit" or contract.get("authorized") is not True:
        raise ValueError("unauthorized all-data final-fit contract")
    if any(row.get("role") != "train" for row in records):
        raise ValueError("all-data final fit accepts train records only")
    if len(records) != 4311 or dict(Counter(row["class_id"] for row in records)) != EXPECTED_COUNTS:
        raise ValueError("all-data count or class counts differ from 4311 / 3142,1055,114")
    identities = [row["record_id"] for row in records]
    paths = [row["source_path"] for row in records]
    if len(set(identities)) != len(records) or len(set(paths)) != len(records):
        raise ValueError("duplicate identity or path in all-data contract")
    observed = {}
    for row in records:
        if verify_content and sha256_file(Path(row["source_path"])) != row["sha256"]:
            raise ValueError(f"content hash mismatch: {row['record_id']}")
        observed[row["record_id"]] = {key: row[key] for key in ("source_path", "class_id", "sha256", "role")}
    if _canonical_hash(observed) != contract.get("items_sha256") or set(observed) != set(contract.get("items", {})):
        raise ValueError("all-data immutable item binding mismatch")
    if hashlib.sha256("\n".join(sorted(identities)).encode()).hexdigest() != contract.get("identities_sha256"):
        raise ValueError("all-data identity hash mismatch")
    return {"status": "accepted", "count": len(records), "class_counts": EXPECTED_COUNTS, "items_sha256": contract["items_sha256"]}


def _parse_phase3c_epochs(ledger: dict[str, Any]) -> dict[str, list[list[int]]]:
    result = {arch: [] for arch in ARCHITECTURES}
    for row in ledger["runs"]:
        events = []
        for line in Path(row["events"]).read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("{"):
                try: events.append(json.loads(line))
                except json.JSONDecodeError: pass
        selected = [int(event["best_epoch"]) for event in events if event.get("event") == "stage_finished"]
        if len(selected) != 2:
            raise ValueError(f"missing Phase-3C stage selection evidence: {row['architecture']} {row['fold']}")
        result[row["architecture"]].append(selected)
    if any(len(folds) != 2 for folds in result.values()):
        raise ValueError("Phase-3C requires exactly two folds per architecture")
    return result


def _config(arch: str, seed: int, epochs: list[int], weights: dict[str, Any], contract: dict[str, Any]) -> dict[str, Any]:
    return {"schema_version": 2, "training_contract": "all_data_final_fit", "architecture": arch, "classes": ["0", "1", "2"],
            "display_labels": {"0": "Empty", "1": "Single", "2": "MoreThanOne"}, "seed": seed, "input_size": [96, 96, 3],
            "normalization": {"mean": [.485, .456, .406], "std": [.229, .224, .225]}, "batch_size": 64, "num_workers": 0,
            "weight_decay": 1e-4, "use_amp": True, "classifier_output": "signed_logits", "classifier_initialization": "deterministic_new_head",
            "pretrained": True, "pretrained_weight_id": weights["weight_id"], "pretrained_weight_path": weights["cache_path"],
            "pretrained_weight_sha256": weights["sha256"], "optimizer": {"name": "adam"}, "scheduler": {"name": "none"},
            "augmentation": {"random_resized_crop": True, "affine": True, "color_jitter": True, "horizontal_flip": False},
            "imbalance": {"mode": "balanced_sampler", "sampler_alpha": .65, "computed_from": "all_4311_training_records"},
            "stages": [{"name": "head_late_blocks", "epochs": epochs[0], "learning_rate": 1e-4, "trainable": "classifier_and_last_blocks"},
                       {"name": "controlled_fine_tune", "epochs": epochs[1], "learning_rate": 1e-5, "trainable": "controlled_fine_tune"}],
            "all_data_contract_sha256": _canonical_hash(contract), "evaluation_prohibited": True, "export_onnx": True, "onnx_opset": 18}


def _fixed_eval_loader(records: list[dict[str, str]], schema: ClassSchema, config: dict[str, Any], device: Any):
    from torch.utils.data import DataLoader
    _, transform = _build_transforms(config)
    return DataLoader(ManifestImageDataset(records, schema.class_to_idx, transform), batch_size=64, shuffle=False, num_workers=0, pin_memory=device.type == "cuda")


def _ort_verify(model: Any, onnx: Path, inputs: list[np.ndarray]) -> dict[str, Any]:
    import onnxruntime as ort
    import torch
    session = ort.InferenceSession(str(onnx), providers=["CPUExecutionProvider"])
    parity_model = copy.deepcopy(model).cpu().eval()
    name = session.get_inputs()[0].name
    parity = []
    with torch.no_grad():
        for array in inputs:
            expected = parity_model(torch.from_numpy(array)).numpy()
            actual = session.run(None, {name: array})[0]
            parity.append(float(np.max(np.abs(expected-actual))))
    for _ in range(20): session.run(None, {name: inputs[0]})
    samples=[]
    for _ in range(200):
        started=time.perf_counter_ns(); session.run(None,{name:inputs[0]}); samples.append((time.perf_counter_ns()-started)/1e6)
    return {"providers": session.get_providers(), "input_shape": session.get_inputs()[0].shape, "output_shape": session.get_outputs()[0].shape,
            "parity_max_abs_error": max(parity), "parity_inputs": len(inputs), "latency": latency_statistics(samples)}


def validate_bundle_manifest(manifest: dict[str, Any]) -> None:
    if manifest.get("schema_version") != "opendss-bundle-candidate-v1" or manifest.get("classes") != ["0", "1", "2"]:
        raise ValueError("invalid bundle candidate schema or class order")
    if manifest.get("input", {}).get("shape") != ["N", 3, 96, 96] or manifest.get("output", {}).get("shape") != ["N", 3]:
        raise ValueError("invalid OpenDSS runtime tensor contract")
    if manifest.get("performance_evidence", {}).get("claim_source") != "cross-validation only":
        raise ValueError("all-data manifest must prohibit evaluation claims")
    artifact=manifest.get("artifact", {})
    if not artifact.get("sha256") or int(artifact.get("total_byte_size", 0)) <= 0:
        raise ValueError("bundle candidate artifact provenance is incomplete")


def train_one(arch: str, config: dict[str, Any], records: list[dict[str, str]], run_root: Path, contract: dict[str, Any]) -> dict[str, Any]:
    import torch
    from torch.utils.data import DataLoader, WeightedRandomSampler
    if run_root.exists(): raise FileExistsError(f"immutable run already exists: {run_root}")
    (run_root/"checkpoints").mkdir(parents=True)
    validate_contract(records, contract, verify_content=True)
    if sha256_file(Path(config["pretrained_weight_path"])) != config["pretrained_weight_sha256"]: raise ValueError("pretrained weight hash changed")
    device=torch.device("cuda"); _seed_everything(int(config["seed"])); torch.cuda.reset_peak_memory_stats(device)
    schema=ClassSchema(schema_id="opendss-3class-v1", classes=["0","1","2"], display_labels=config["display_labels"])
    train_tf,_=_build_transforms(config); dataset=ManifestImageDataset(records,schema.class_to_idx,train_tf)
    weights=[EXPECTED_COUNTS[row["class_id"]]**(-.65) for row in records]; generator=torch.Generator().manual_seed(int(config["seed"]))
    loader=DataLoader(dataset,batch_size=64,sampler=WeightedRandomSampler(weights,len(weights),replacement=True,generator=generator),num_workers=0,pin_memory=True)
    model=_build_model(config,3).to(device); criterion=torch.nn.CrossEntropyLoss(); history=[]; total_updates=0; started=time.monotonic(); stage_artifacts=[]
    for stage in config["stages"]:
        _freeze_for_stage(model,stage["trainable"]); initial={n:v.detach().cpu().clone() for n,v in model.state_dict().items()}; optimizer=_make_optimizer(model,config,float(stage["learning_rate"])); scaler=_make_cuda_amp_scaler(torch)
        for epoch in range(1,int(stage["epochs"])+1):
            result=_run_epoch(model,loader,criterion,optimizer,device,scaler); total_updates+=len(loader); counts=dict(Counter(str(v) for v in result["targets"])); delta=_parameter_delta_l2(initial,model)
            history.append({"stage":stage["name"],"epoch":epoch,"train_loss":result["loss"],"train_accuracy_diagnostic":sum(int(a==b) for a,b in zip(result["targets"],result["preds"]))/len(result["targets"]),"sampled_class_counts":counts,"optimizer_updates":total_updates,"parameter_delta_l2":delta,"elapsed_seconds":time.monotonic()-started,"learning_rate":optimizer.param_groups[0]["lr"]})
        stage_path=run_root/"checkpoints"/f"{stage['name']}_final.pth"; payload={"model_state":copy.deepcopy(model.state_dict()),"config":config,"identity":"stage_final_no_validation","stage":stage["name"],"stage_epoch":stage["epochs"],"optimizer_state":optimizer.state_dict(),"scaler_state":scaler.state_dict(),"optimizer_updates":total_updates}; torch.save(payload,stage_path)
        stage_artifacts.append({"stage":stage["name"],"path":str(stage_path),"sha256":sha256_file(stage_path),"trainable":_trainable_parameter_summary(model),"parameter_delta_l2":history[-1]["parameter_delta_l2"]})
    final=run_root/"checkpoints"/"true_final.pth"; torch.save({"model_state":copy.deepcopy(model.state_dict()),"config":config,"identity":"true_final_all_data_no_validation","optimizer_updates":total_updates,"history":history},final)
    onnx,opset=_export_onnx(model,run_root,config)
    eval_loader=_fixed_eval_loader(records,schema,config,device); targets=[];preds=[];logits=[]
    model.eval()
    with torch.no_grad():
        for images,labels in eval_loader:
            out=model(images.to(device)); targets.extend(labels.tolist());preds.extend(out.argmax(1).cpu().tolist());logits.extend(out.cpu().tolist())
    diagnostics=_logit_diagnostics(logits,preds,3); diagnostics.update({"scope":"training_data_diagnostic_only","rows":len(targets),"prediction_distribution":dict(Counter(str(v) for v in preds))})
    fixed=[]
    for images,_ in eval_loader:
        fixed.extend([images[i:i+1].numpy().astype(np.float32) for i in range(min(4,len(images)))]); break
    ort=_ort_verify(model,onnx,fixed)
    telemetry={"elapsed_seconds":time.monotonic()-started,"optimizer_updates":total_updates,"device":"cuda","device_name":torch.cuda.get_device_name(device),"peak_cuda_memory_allocated":int(torch.cuda.max_memory_allocated(device)),"peak_cuda_memory_reserved":int(torch.cuda.max_memory_reserved(device))}
    architecture_name={"mobilenet_v3_small":"OpenDSS MobileNetV3 Small 3-Class","efficientnet_b0":"OpenDSS EfficientNet B0 3-Class"}[arch]
    sidecars=[{"file":p.name,"sha256":sha256_file(p),"byte_size":p.stat().st_size} for p in run_root.glob("*.onnx.data")]
    total_onnx_bytes=onnx.stat().st_size+sum(row["byte_size"] for row in sidecars)
    manifest={"schema_version":"opendss-bundle-candidate-v1","model_id":f"production-{arch}-3class","model_name":architecture_name,"architecture":arch,"classes":["0","1","2"],"display_labels":config["display_labels"],"input":{"shape":["N",3,96,96],"layout":"NCHW","dtype":"float32","normalization":config["normalization"]},"output":{"shape":["N",3],"meaning":"signed logits in class order"},"artifact":{"onnx_file":"model.onnx","sha256":sha256_file(onnx),"byte_size":onnx.stat().st_size,"total_byte_size":total_onnx_bytes,"external_data_files":sidecars,"opset":opset},"training":{"contract":"all_data_final_fit","records":4311,"class_counts":EXPECTED_COUNTS,"config_sha256":_canonical_hash(config),"checkpoint_sha256":sha256_file(final)},"performance_evidence":{"report":"docs/worker-reports/publication-gpu-training-experiments-2026-07-18/phase-3c-architecture-comparison.md","claim_source":"cross-validation only"},"limitations":["All-data predictions are training diagnostics, not generalization estimates.","Not registered or activated."]}
    validate_bundle_manifest(manifest)
    (run_root/"bundle_candidate_manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    result={"architecture":arch,"status":"accepted_candidate","config":config,"config_sha256":_canonical_hash(config),"final_checkpoint":str(final),"final_checkpoint_sha256":sha256_file(final),"onnx":str(onnx),"onnx_sha256":sha256_file(onnx),"onnx_size_bytes":onnx.stat().st_size,"onnx_total_size_bytes":total_onnx_bytes,"onnx_external_data_files":sidecars,"stages":stage_artifacts,"history":history,"training_diagnostics":diagnostics,"onnx_verification":ort,"telemetry":telemetry,"bundle_manifest":str(run_root/"bundle_candidate_manifest.json"),"bundle_manifest_sha256":sha256_file(run_root/"bundle_candidate_manifest.json"),"evaluation_metrics_computed":False}
    (run_root/"result.json").write_text(json.dumps(result,indent=2),encoding="utf-8"); return result


def execute(root: Path, phase3c: Path, repo: Path) -> dict[str, Any]:
    derived=root/"prep"/"derived_manifest.json"; folds=root/"prep"/"fold_manifest.json"; ledger=json.loads(phase3c.read_text(encoding="utf-8")); records=build_records(derived); contract=make_contract(records,derived,folds)
    selected_epochs=_parse_phase3c_epochs(ledger); schedules=derive_schedule(selected_epochs); weights={arch:weight_provenance(arch) for arch in ARCHITECTURES}
    execution=root/"production-candidates"/("final-all-data-"+datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")); execution.mkdir(parents=True)
    plan={"schema_version":1,"execution":str(execution),"authorized_training_runs":2,"architectures":list(ARCHITECTURES),"duration_rule":"upper-rounded median of the two Phase-3C selected stage epochs, independently per architecture/stage","duration_inputs":selected_epochs,"schedules":schedules,"contract":contract,"weights":weights,"phase3c_ledger":str(phase3c),"phase3c_ledger_sha256":sha256_file(phase3c),"code_hashes":{"train.py":sha256_file(repo/"training/python/droplet_trainer/train.py"),"runner":sha256_file(Path(__file__))},"evaluation_prohibited":True,"integration_authorized":False}
    (execution/"immutable_plan.json").write_text(json.dumps(plan,indent=2),encoding="utf-8")
    preflights=[]
    for arch in ARCHITECTURES: preflights.append({"architecture":arch,**validate_contract(records,contract,True),"model_created":False,"optimizer_created":False,"optimizer_updates":0})
    corrupt=copy.deepcopy(records); corrupt[0]["class_id"]="2"
    try: validate_contract(corrupt,contract,False); raise AssertionError("corrupt contract unexpectedly accepted")
    except ValueError as exc: negative={"status":"rejected_as_required","error":str(exc),"model_created":False,"optimizer_created":False,"optimizer_updates":0}
    (execution/"preflight_results.json").write_text(json.dumps({"positive":preflights,"negative":negative},indent=2),encoding="utf-8")
    results=[]
    for arch,seed in (("mobilenet_v3_small",1729),("efficientnet_b0",2718)):
        config=_config(arch,seed,schedules[arch],weights[arch],contract); results.append(train_one(arch,config,records,execution/arch,contract))
    final={"execution":str(execution),"plan_sha256":sha256_file(execution/"immutable_plan.json"),"preflight_sha256":sha256_file(execution/"preflight_results.json"),"completed_training_runs":len(results),"results":results,"evaluation_claims":False,"integration_performed":False}
    (execution/"run_ledger.json").write_text(json.dumps(final,indent=2),encoding="utf-8"); (root/"production-candidates"/"latest.json").write_text(json.dumps({"execution":str(execution),"ledger":str(execution/"run_ledger.json")},indent=2),encoding="utf-8"); return final


def refresh_artifact_verification(execution: Path) -> dict[str, Any]:
    """Recompute deployment verification only; checkpoints and ONNX are immutable."""
    import torch
    plan=json.loads((execution/"immutable_plan.json").read_text(encoding="utf-8")); records=list(plan["contract"]["items"].values())
    records=[{"record_id":identity,**row} for identity,row in plan["contract"]["items"].items()]
    ledger=json.loads((execution/"run_ledger.json").read_text(encoding="utf-8"))
    for result in ledger["results"]:
        run_root=execution/result["architecture"]; checkpoint=Path(result["final_checkpoint"]); payload=torch.load(checkpoint,map_location="cpu",weights_only=False); model=_build_model(payload["config"],3);model.load_state_dict(payload["model_state"],strict=True);model.eval()
        schema=ClassSchema(schema_id="opendss-3class-v1",classes=["0","1","2"],display_labels=payload["config"]["display_labels"]); loader=_fixed_eval_loader(records,schema,payload["config"],torch.device("cpu")); images,_=next(iter(loader)); fixed=[images[i:i+1].numpy().astype(np.float32) for i in range(4)]
        onnx=Path(result["onnx"]); result["onnx_verification"]=_ort_verify(model,onnx,fixed); sidecars=[{"file":p.name,"sha256":sha256_file(p),"byte_size":p.stat().st_size} for p in run_root.glob("*.onnx.data")]; total=onnx.stat().st_size+sum(row["byte_size"] for row in sidecars);result["onnx_total_size_bytes"]=total;result["onnx_external_data_files"]=sidecars
        manifest_path=Path(result["bundle_manifest"]);manifest=json.loads(manifest_path.read_text(encoding="utf-8"));manifest["artifact"]["total_byte_size"]=total;manifest["artifact"]["external_data_files"]=sidecars;manifest["artifact_verification"]=result["onnx_verification"];validate_bundle_manifest(manifest);manifest_path.write_text(json.dumps(manifest,indent=2),encoding="utf-8");result["bundle_manifest_sha256"]=sha256_file(manifest_path)
        (run_root/"result.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    ledger["artifact_verification_refreshed_without_training"]=True;ledger["artifact_verification_code_sha256"]=sha256_file(Path(__file__));(execution/"run_ledger.json").write_text(json.dumps(ledger,indent=2),encoding="utf-8");return ledger


def main(argv=None)->int:
    parser=argparse.ArgumentParser();parser.add_argument("--root",type=Path);parser.add_argument("--phase3c-ledger",type=Path);parser.add_argument("--repo",type=Path);parser.add_argument("--refresh-artifacts",type=Path);args=parser.parse_args(argv)
    if args.refresh_artifacts: result=refresh_artifact_verification(args.refresh_artifacts.resolve())
    else:
        if not args.root or not args.phase3c_ledger or not args.repo: parser.error("training requires --root, --phase3c-ledger, and --repo")
        result=execute(args.root.resolve(),args.phase3c_ledger.resolve(),args.repo.resolve())
    print(json.dumps(result,indent=2));return 0

if __name__=="__main__":raise SystemExit(main())

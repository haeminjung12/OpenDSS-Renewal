from __future__ import annotations

import argparse, json, os, subprocess
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .screen import attach_frozen_contract, base_config, create_immutable_execution_root, load_inputs, manifest_for_fold, parse_events, recompute_retained_metrics, sha256_file

ROOT_NAME = "class2-readiness"
CONDITIONS = [
    {"name":"balanced_sampler_only","classifier_initialization":"partial_mean_existing","imbalance":{"mode":"balanced_sampler"}},
    {"name":"focal_gamma2","classifier_initialization":"partial_mean_existing","imbalance":{"mode":"focal_loss","gamma":2.0}},
    {"name":"effective_cap10","classifier_initialization":"partial_mean_existing","imbalance":{"mode":"effective_number","beta":0.999,"cap":10.0}},
    {"name":"balanced_sampler_effective_cap5","classifier_initialization":"partial_mean_existing","imbalance":{"mode":"balanced_sampler_effective_number","beta":0.999,"cap":5.0}},
    {"name":"copy_class1_effective_cap10","classifier_initialization":"partial_copy_class1","imbalance":{"mode":"effective_number","beta":0.999,"cap":10.0}},
]


def audit(root: Path, repo: Path, python: Path, source_model: Path) -> dict[str, Any]:
    import numpy as np
    import torch
    import torch.nn as nn
    from PIL import Image, ImageDraw
    from torch.utils.data import DataLoader
    from droplet_trainer.schema import ClassSchema
    from droplet_trainer.train import ManifestImageDataset, _build_model, _build_transforms, _classification_metrics, _evaluate, _load_source_artifact, _seed_everything
    derived, folds, _, _ = load_inputs(root / "prep"); fold = next(f for f in folds["folds"] if int(f["fold"]) == 0)
    records = {r["record_id"]: r for r in derived["records"]}; execution_id = "audit-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    out = create_immutable_execution_root(root / ROOT_NAME, execution_id)
    rows, source_counts, modes = [], Counter(), Counter()
    for role, ids in (("train", fold["development"]), ("validation", fold["evaluation"])):
        for identity in ids:
            r = records[identity]
            if r["class_id"] != "2": continue
            path = Path(r["source_root"]) / r["source_path"]
            with Image.open(path) as image:
                array = np.asarray(image.convert("L"), dtype=np.float32)
                modes[image.mode] += 1
            source_counts[str(r.get("source_dataset_id", r.get("source_root")))] += 1
            rows.append({"record_id": identity, "role": role, "path": str(path), "sha256": sha256_file(path), "provenance": {k: r.get(k) for k in ("source_dataset_id", "source_item_id", "source_root", "source_path")}, "pixel_mean": float(array.mean()), "pixel_std": float(array.std()), "pixel_min": int(array.min()), "pixel_max": int(array.max())})
    def sheet(role: str) -> str:
        selected = [r for r in rows if r["role"] == role]; tile = 88; cols = 8; canvas = Image.new("RGB", (cols*tile, ((len(selected)+cols-1)//cols)*tile), "white"); draw = ImageDraw.Draw(canvas)
        for i, row in enumerate(selected):
            with Image.open(row["path"]) as im: thumb = im.convert("RGB").resize((64,64))
            x=(i%cols)*tile; y=(i//cols)*tile; canvas.paste(thumb,(x,y)); draw.text((x,y+65),row["record_id"][-10:],fill="black")
        path=out/f"class2_{role}_contact_sheet.png"; canvas.save(path); return str(path)
    schema=ClassSchema(["0","1","2"],{"0":"Empty","1":"Single","2":"MoreThanOne"},schema_id="publication-3class-v1"); config=base_config("staged_signed_1e-4_1e-5",1729,source_model); _seed_everything(1729)
    _,tf=_build_transforms(config); val_items=[]
    for identity in fold["evaluation"]:
        r=records[identity]; val_items.append({"record_id":identity,"source_path":str(Path(r["source_root"])/r["source_path"]),"class_id":r["class_id"]})
    loader=DataLoader(ManifestImageDataset(val_items,schema.class_to_idx,tf),batch_size=64,shuffle=False,num_workers=0); device=torch.device("cuda")
    model=_build_model(config,3); load_notes=_load_source_artifact(model,str(source_model),None); classifier=model.classifier[1]; weights=classifier.weight.detach().flatten(1); bias=classifier.bias.detach(); model=model.to(device)
    evaluated=_evaluate(model,loader,nn.CrossEntropyLoss(),device); metrics=_classification_metrics(evaluated["targets"],evaluated["preds"],schema.classes)
    logits=np.asarray(evaluated["logits"]); targets=np.asarray(evaluated["targets"]); per_true={str(c): {"mean_logits":logits[targets==c].mean(0).tolist(),"predictions":dict(Counter(np.argmax(logits[targets==c],axis=1).tolist()))} for c in range(3)}
    embeddings=[]; embedding_targets=[]
    model.eval()
    with torch.no_grad():
        for images, labels in loader:
            features=model.features(images.to(device)); pooled=torch.nn.functional.adaptive_avg_pool2d(features,(1,1)).flatten(1); embeddings.append(pooled.cpu().numpy()); embedding_targets.extend(labels.tolist())
    embedding_array=np.concatenate(embeddings); embedding_targets_array=np.asarray(embedding_targets); centroids={str(c):embedding_array[embedding_targets_array==c].mean(0) for c in range(3)}
    centroid_distances={f"{a}-{b}":float(np.linalg.norm(centroids[str(a)]-centroids[str(b)])) for a in range(3) for b in range(a+1,3)}
    within_scatter={str(c):float(np.mean(np.linalg.norm(embedding_array[embedding_targets_array==c]-centroids[str(c)],axis=1))) for c in range(3)}
    phase2b_checkpoint=root/"screening-readiness"/"phase2b-canary-20260719T032123540994Z"/"trainer"/"canary"/"checkpoints"/"best.pth"
    phase2b_summary=None
    if phase2b_checkpoint.exists():
        checkpoint=torch.load(phase2b_checkpoint,map_location=device,weights_only=False); model.load_state_dict(checkpoint["model_state"],strict=True); prior=_evaluate(model,loader,nn.CrossEntropyLoss(),device); prior_metrics=_classification_metrics(prior["targets"],prior["preds"],schema.classes); prior_logits=np.asarray(prior["logits"]); phase2b_summary={"checkpoint":str(phase2b_checkpoint),"sha256":sha256_file(phase2b_checkpoint),"metrics":prior_metrics,"class2_mean_logits":prior_logits[targets==2].mean(0).tolist(),"class2_predictions":dict(Counter(np.argmax(prior_logits[targets==2],axis=1).tolist()))}
    # Balanced diagnostic batch: per-class loss and classifier gradient norms.
    selected=[]
    for c in range(3): selected.extend([item for item in val_items if item["class_id"]==str(c)][:16])
    batch=next(iter(DataLoader(ManifestImageDataset(selected,schema.class_to_idx,tf),batch_size=len(selected),shuffle=False))); images,labels=batch[0].to(device),batch[1].to(device); model.zero_grad(); outputs=model(images)
    losses={}; gradients={}
    for c in range(3): losses[str(c)]=float(nn.functional.cross_entropy(outputs[labels==c],labels[labels==c]).item())
    nn.functional.cross_entropy(outputs,labels).backward(); gradients={"classifier_weight_l2":float(classifier.weight.grad.norm().item()),"classifier_bias_l2":float(classifier.bias.grad.norm().item())}
    result={"execution_id":execution_id,"fold":0,"seed":1729,"class2_counts":dict(Counter(r["role"] for r in rows)),"source_counts":dict(source_counts),"image_modes":dict(modes),"pixel_summary":{"mean":float(np.mean([r["pixel_mean"] for r in rows])),"std_mean":float(np.mean([r["pixel_std"] for r in rows]))},"records":rows,"contact_sheets":{"train":sheet("train"),"validation":sheet("validation")},"source_model_sha256":sha256_file(source_model),"source_load_notes":load_notes,"classifier":{"weight_norms":[float(v) for v in weights.norm(dim=1)],"bias":[float(v) for v in bias],"cosine_similarity":torch.nn.functional.cosine_similarity(weights[:,None,:],weights[None,:,:],dim=2).tolist()},"source_validation_metrics":metrics,"per_true_class_logits":per_true,"embedding_analysis":{"centroid_l2_distances":centroid_distances,"mean_within_class_scatter":within_scatter},"phase2b_selected_checkpoint":phase2b_summary,"balanced_batch":{"items_per_class":16,"per_class_unweighted_ce":losses,"gradients":gradients},"training_runs":0}
    (out/"audit.json").write_text(json.dumps(result,indent=2),encoding="utf-8"); return result


def freeze_plan(root: Path) -> dict:
    audit_roots=sorted((root/ROOT_NAME).glob("audit-*")); audit_path=audit_roots[-1]/"audit.json"; audit_doc=json.loads(audit_path.read_text(encoding="utf-8"))
    plan={"schema_version":1,"frozen_at":datetime.now(timezone.utc).isoformat(),"audit":str(audit_path),"audit_sha256":sha256_file(audit_path),"finding":"Pinned source has a two-output classifier; uncorrected 3-class import skips classifier tensors and leaves class 2 suppressed. All readiness conditions explicitly use partial classifier import.","fixed_fold":0,"fixed_seed":1729,"maximum_runs":10,"pruning":"Stop a condition after one run if any prediction class is absent or class 0/1 recall <0.50. Confirm only conditions with class2 recall >0 and all classes present.","ordered_conditions":CONDITIONS,"base_stages":[{"name":"readiness_stage1","epochs":3,"learning_rate":1e-4,"trainable":"classifier_and_last_fire_modules"},{"name":"readiness_stage2","epochs":2,"learning_rate":1e-5,"trainable":"fine_tune"}],"training_started":False}
    target=root/ROOT_NAME/"frozen_condition_plan.json"; target.write_text(json.dumps(plan,indent=2),encoding="utf-8"); return plan


def run_condition(root:Path,repo:Path,python:Path,source_model:Path,condition:dict,fold_index:int,seed:int,stages:list[dict],sequence:int)->dict:
    derived,folds,_,_=load_inputs(root/"prep"); fold=next(f for f in folds["folds"] if int(f["fold"])==fold_index); execution_id=f"{sequence:02d}-{condition['name']}-seed{seed}-fold{fold_index}-"+datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ"); run_root=create_immutable_execution_root(root/ROOT_NAME/"runs",execution_id)
    manifest=manifest_for_fold(derived,fold); dataset=run_root/"dataset"; (dataset/"metadata").mkdir(parents=True); manifest_path=dataset/"metadata"/"dataset_manifest.json"; manifest_path.write_text(json.dumps(manifest,indent=2),encoding="utf-8")
    config=base_config("staged_signed_1e-4_1e-5",seed,source_model); config["classifier_output"]="signed_logits"; config["classifier_initialization"]=condition["classifier_initialization"]; config["imbalance"]={**condition["imbalance"],"class_weight_formula":"condition_specific","computed_from":"training_split_only","class_weights":{}}; config["stages"]=stages; config=attach_frozen_contract(config,manifest,manifest_path,root/"prep",fold); config_path=run_root/"immutable_config.json"; config_path.write_text(json.dumps(config,indent=2),encoding="utf-8")
    env=os.environ.copy(); env["PYTHONPATH"]=str(repo/"training"/"python"); output=run_root/"trainer"; cmd=[str(python),"-m","droplet_trainer","train","--dataset",str(dataset),"--output",str(output),"--config",str(config_path),"--device","cuda","--classes","0,1,2","--run-name","run","--jsonl"]
    started=datetime.now(timezone.utc); cp=subprocess.run(cmd,cwd=repo,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding="utf-8",errors="replace"); wall=(datetime.now(timezone.utc)-started).total_seconds(); events_path=run_root/"events.jsonl"; events_path.write_text(cp.stdout,encoding="utf-8"); events=parse_events(events_path); trainer=output/"run"; metrics_path=trainer/"metrics.json"; failure=trainer/"failure_diagnostics.json"
    if metrics_path.exists(): doc=json.loads(metrics_path.read_text(encoding="utf-8")); metrics=doc["test"]
    elif failure.exists(): metrics=json.loads(failure.read_text(encoding="utf-8"))["metrics"]
    else: metrics={}
    epoch_rows=[e.get("metrics",{}) for e in events if e.get("event")=="epoch_metrics"]; updates=max((int(r.get("optimizer_updates",0)) for r in epoch_rows),default=0); delta=max((float(r.get("parameter_delta_l2",0)) for r in epoch_rows),default=0); predictions=trainer/"validation_predictions.jsonl"; recomputed=recompute_retained_metrics(predictions,metrics) if predictions.exists() and metrics else None; recalls=[float(r["recall"]) for r in metrics.get("class_metrics",[])]; distribution=metrics.get("logit_diagnostics",{}).get("prediction_distribution",{}); basic=cp.returncode==0 and len(recalls)==3 and min(recalls)>0 and len([v for v in distribution.values() if v])==3 and recalls[0]>=.5 and recalls[1]>=.5
    result={"execution_id":execution_id,"condition":condition,"seed":seed,"fold":fold_index,"status":"viable" if basic else "pruned","exit_code":cp.returncode,"wall_seconds":wall,"optimizer_updates":updates,"parameter_delta_l2":delta,"balanced_accuracy":metrics.get("balanced_accuracy"),"macro_f1":metrics.get("macro_f1"),"recalls":recalls,"prediction_distribution":distribution,"diagnostics":metrics.get("logit_diagnostics"),"telemetry":metrics.get("telemetry"),"metric_recomputation":recomputed,"checkpoint_hashes":{p.name:sha256_file(p) for p in (trainer/"checkpoints").glob("*.pth")},"events":str(events_path),"trainer_dir":str(trainer),"publication_result":False}
    (run_root/"result.json").write_text(json.dumps(result,indent=2),encoding="utf-8"); return result


def execute_plan(root:Path,repo:Path,python:Path,source_model:Path)->dict:
    plan_path=root/ROOT_NAME/"frozen_condition_plan.json"; plan=json.loads(plan_path.read_text(encoding="utf-8")); ledger=[]; stages=plan["base_stages"]
    for index,condition in enumerate(plan["ordered_conditions"],1):
        ledger.append(run_condition(root,repo,python,source_model,condition,0,1729,stages,index))
    viable=[row for row in ledger if row["status"]=="viable"]
    # Confirm at most two best on the second fixed binding.
    viable.sort(key=lambda r:(r.get("balanced_accuracy") or -1,r.get("macro_f1") or -1),reverse=True)
    for offset,parent in enumerate(viable[:2],len(ledger)+1): ledger.append(run_condition(root,repo,python,source_model,parent["condition"],1,2718,stages,offset))
    confirmed=[]
    for first in viable[:2]:
        matches=[r for r in ledger if r["condition"]["name"]==first["condition"]["name"] and r["fold"]==1 and r["status"]=="viable"]
        if matches: confirmed.append(first["condition"]["name"])
    result={"schema_version":1,"plan_sha256":sha256_file(plan_path),"attempted_runs":len(ledger),"maximum_runs":10,"runs":ledger,"confirmed_parents":confirmed,"phase3_launched":False}; ledger_path=root/ROOT_NAME/"run_ledger.json"; ledger_path.write_text(json.dumps(result,indent=2),encoding="utf-8"); return result


def main(argv=None)->int:
    p=argparse.ArgumentParser(); p.add_argument("--root",type=Path,required=True); p.add_argument("--repo",type=Path,required=True); p.add_argument("--python",type=Path,required=True); p.add_argument("--source-model",type=Path,required=True); p.add_argument("--freeze-plan",action="store_true"); p.add_argument("--execute-plan",action="store_true"); args=p.parse_args(argv); root=args.root.resolve()
    if args.freeze_plan: result=freeze_plan(root)
    elif args.execute_plan: result=execute_plan(root,args.repo.resolve(),args.python.resolve(),args.source_model.resolve())
    else: result=audit(root,args.repo.resolve(),args.python.resolve(),args.source_model.resolve())
    print(json.dumps(result,indent=2)); return 0

if __name__=="__main__": raise SystemExit(main())

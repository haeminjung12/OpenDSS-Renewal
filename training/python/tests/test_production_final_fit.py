from __future__ import annotations
import copy
import pytest
from droplet_trainer.publication_experiments.production_final_fit import _config, derive_schedule, validate_bundle_manifest, validate_contract

def test_duration_rule_is_upper_median_per_architecture_stage():
    assert derive_schedule({"m":[[7,2],[14,2]],"e":[[8,1],[6,6]]})=={"m":[11,2],"e":[7,4]}

def test_all_data_contract_rejects_corruption(tmp_path):
    records=[]
    for class_id,count in {"0":3142,"1":1055,"2":114}.items():
        for index in range(count): records.append({"record_id":f"{class_id}-{index}","source_path":str(tmp_path/f"{class_id}-{index}.png"),"class_id":class_id,"sha256":"x","role":"train"})
    contract={"mode":"all_data_final_fit","authorized":True,"items":{r["record_id"]:{k:r[k] for k in ("source_path","class_id","sha256","role")} for r in records}}
    from droplet_trainer.publication_experiments.production_final_fit import _canonical_hash
    contract["items_sha256"]=_canonical_hash(contract["items"]);contract["identities_sha256"]=__import__('hashlib').sha256("\n".join(sorted(contract["items"])).encode()).hexdigest()
    validate_contract(records,contract,False)
    corrupt=copy.deepcopy(records);corrupt[0]["class_id"]="2"
    with pytest.raises(ValueError,match="count|binding"):validate_contract(corrupt,contract,False)

def test_final_fit_config_has_fixed_no_validation_behavior():
    cfg=_config("mobilenet_v3_small",1729,[11,2],{"weight_id":"w","cache_path":"p","sha256":"h"},{"x":1})
    assert cfg["scheduler"]=={"name":"none"}
    assert cfg["evaluation_prohibited"] is True
    assert [stage["epochs"] for stage in cfg["stages"]]==[11,2]

def test_bundle_manifest_requires_runtime_contract_and_cv_only_claims():
    manifest={"schema_version":"opendss-bundle-candidate-v1","classes":["0","1","2"],"input":{"shape":["N",3,96,96]},"output":{"shape":["N",3]},"performance_evidence":{"claim_source":"cross-validation only"},"artifact":{"sha256":"a","total_byte_size":1}}
    validate_bundle_manifest(manifest)
    manifest["performance_evidence"]["claim_source"]="all-data accuracy"
    with pytest.raises(ValueError,match="prohibit"):validate_bundle_manifest(manifest)

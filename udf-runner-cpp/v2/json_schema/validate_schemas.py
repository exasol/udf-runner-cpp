import copy
import json
from pathlib import Path

from jsonschema import Draft7Validator, ValidationError
from referencing import Registry, Resource
from referencing.jsonschema import DRAFT7


ROOT = Path(__file__).resolve().parent
EXAMPLES = ROOT.parents[2] / "doc" / "design" / "v2" / "protocol" / "examples"


def load_schemas() -> dict[str, dict]:
    schemas = {
        path.name: json.loads(path.read_text())
        for path in ROOT.glob("*.schema.json")
    }
    for schema in schemas.values():
        Draft7Validator.check_schema(schema)
    return schemas


def validate(schemas: dict[str, dict], schema_name: str, instance: object) -> None:
    schema_path = ROOT / schema_name
    resources = []
    for name, schema in schemas.items():
        resource = Resource.from_contents(schema, default_specification=DRAFT7)
        resources.extend(((name, resource), ((ROOT / name).as_uri(), resource)))
    registry = Registry().with_resources(resources)

    root_schema = copy.deepcopy(schemas[schema_name])
    root_schema["$id"] = schema_path.as_uri()
    Draft7Validator(root_schema, registry=registry).validate(instance)


def read_example(name: str) -> object:
    return json.loads((EXAMPLES / name).read_text())


def validate_mapping_examples() -> None:
    decimal = read_example("decimal_field_metadata.json")
    assert decimal["metadata"]["exasol:type_family"] == "DECIMAL"
    precision = int(decimal["metadata"]["exasol:precision"])
    scale = int(decimal["metadata"]["exasol:scale"])
    assert 0 <= scale <= precision

    timestamp = read_example("timestamp_field_metadata.json")
    assert timestamp["metadata"]["exasol:type_family"] == "TIMESTAMP WITH LOCAL TIME ZONE"

    hashtype = read_example("hashtype_field_metadata.json")
    assert 1 <= int(hashtype["metadata"]["exasol:hash_byte_width"]) <= 1024

    year_month = read_example("year_month_interval_field_metadata.json")
    assert year_month["metadata"]["ARROW:extension:name"] == "exasol.interval.year_month"

    day_time = read_example("day_time_interval_field_metadata.json")
    assert day_time["metadata"]["exasol:type_family"] == "INTERVAL DAY TO SECOND"

    geometry = read_example("geometry_extension_metadata.json")
    assert geometry["ARROW:extension:name"] == "geoarrow.wkb"
    extension_metadata = json.loads(geometry["ARROW:extension:metadata"])
    assert extension_metadata["crs_type"] == "srid"
    assert extension_metadata["crs"] == geometry["exasol:srid"]


def validate_schemas() -> None:
    schemas = load_schemas()
    call_metadata = read_example("call_metadata.json")
    column_definitions = read_example("column_definitions.json")
    import_specification = read_example("import_specification.json")

    validate(schemas, "call_metadata.schema.json", call_metadata)
    validate(
        schemas,
        "import_specification.schema.json",
        {"is_subselect": True, "subselect_column_specification": column_definitions},
    )
    validate(schemas, "import_specification.schema.json", import_specification)
    validate(schemas, "export_specification.schema.json", read_example("export_specification.json"))
    validate(schemas, "connection_information.schema.json", read_example("connection_information.json"))
    validate_mapping_examples()

    invalid = copy.deepcopy(call_metadata)
    invalid["input_columns"][0]["type"] = "STRING"
    try:
        validate(schemas, "call_metadata.schema.json", invalid)
    except ValidationError:
        pass
    else:
        raise AssertionError("legacy STRING type was accepted")

    print("v2 JSON schema validation passed")


if __name__ == "__main__":
    validate_schemas()

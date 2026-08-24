# UDF Protocol v2: High-Level Exasol-to-Arrow Type Mapping

This document is the normative high-level type-conversion contract for v2 data-stream column schemas. The protocol uses a
self-owned subset of Arrow's schema model in `udf_protocol.fbs`; it does not import Arrow's FlatBuffers schema.
`Field.custom_metadata` carries logical-type and extension annotations as string key/value pairs.

The conversion is performed from the declared Exasol column type, never from values observed in a batch. A field's
`nullable` flag is preserved independently of its physical type. A conversion that is not defined here fails schema
conversion with a descriptive error.

The [mapping diagram](high_level_type_mapping.svg) summarizes the decision paths below.

## Official Exasol type mappings

`type` in v2 payload metadata identifies one of Exasol's official type families. `type_name` is the canonical,
complete SQL declaration, including parameters. Arrow types in the table are physical protocol representations, not
alternate Exasol type names.

| Exasol type family | Arrow-compatible representation | Contract |
| --- | --- | --- |
| `DOUBLE PRECISION` | `FloatingPoint(Double)` | Preserve nullable field semantics. |
| `DECIMAL` | signed `Int(32)`, signed `Int(64)`, or `Decimal(128)` | Apply declared precision and scale rules below; integer types are storage choices only. |
| `TIMESTAMP` | `Timestamp(unit, "")` | Select the smallest unit preserving declared precision. |
| `TIMESTAMP WITH LOCAL TIME ZONE` | `Timestamp(unit, "UTC")` | Normalize Exasol's UTC-normalized value for transport. |
| `DATE` | `Date(Day)` | Preserve calendar-day semantics. |
| `CHAR` | `Utf8` | Preserve fixed-length padding and character metadata as Exasol semantics. |
| `VARCHAR` | `Utf8` | Preserve declared character length and character metadata. |
| `BOOLEAN` | `Bool` | Preserve nullable values. |
| `HASHTYPE` | `FixedSizeBinary` | Preserve declared byte width and transport raw bytes. |
| `GEOMETRY` | `Binary` + `geoarrow.wkb` | Transport WKB and preserve SRID metadata. |
| `INTERVAL YEAR TO MONTH` | `Struct<years: Int32, months: Int32>` + `exasol.interval.year_month` | Preserve the complete Exasol year/month range. |
| `INTERVAL DAY TO SECOND` | `Interval(MonthDayNano)` | Encode zero months, signed days, and nanoseconds. |

The type family and source declaration metadata use the following field metadata keys:

| Key | Value |
| --- | --- |
| `exasol:type_name` | Original Exasol type declaration or type name. |
| `exasol:type_family` | Official Exasol type family from the table above. |
| `exasol:precision` | Decimal or timestamp precision, when declared. |
| `exasol:scale` | Decimal scale, when declared. |
| `exasol:character_length` | Declared `CHAR`/`VARCHAR` character length, when applicable. |
| `exasol:character_set` | Declared `ASCII` or `UTF8` character set, when applicable. |
| `exasol:srid` | Decimal SRID text, when applicable. |
| `exasol:hash_byte_width` | Canonical fixed-width byte count, when applicable. |

These keys are diagnostic and reconstruction metadata. They do not override the Arrow-compatible physical type.

The high-level JSON column definition supplies conversion parameters without requiring an implementation to parse
`type_name`. Existing properties retain their API meaning: `size` is the character length for `CHAR`/`VARCHAR` or the
declared hash size for `HASHTYPE`; `precision` is decimal, timestamp, or interval leading-field precision; and
`scale` is reserved for decimal scale. `size_unit` distinguishes `BYTE` from `BIT` for `HASHTYPE`.
`character_set`, `srid`, and `fractional_second_precision` provide the additional parsed parameters needed by the
string, geometry, and day-to-second interval mappings. `type_name` remains the complete source declaration for
diagnostics and reconstruction.

### Legacy v1 category replacement

The v1 protocol used `DOUBLE`, `INT32`, `INT64`, `NUMERIC`, `TIMESTAMP`, and `STRING` as internal categories, and
`UNSUPPORTED` as an error sentinel. They are not Exasol SQL types and are not valid v2 `type` values. Their v2
replacements are `DOUBLE PRECISION`, `DECIMAL`, `TIMESTAMP` or `TIMESTAMP WITH LOCAL TIME ZONE`, and `CHAR` or
`VARCHAR`. `INT32` and `INT64` may still appear only as Arrow physical storage choices when a declared
`DECIMAL(p,0)` range safely narrows; `UNSUPPORTED` is a conversion failure, never a type.

## Concrete Exasol SQL mappings

### Numeric values

Exasol defines `DECIMAL(p,s)` with `1 <= p <= 36`, `0 <= s <= 36`, and `s <= p`; the defaults are `p = 18` and
`s = 0`. The declared range determines the representation:

- `DOUBLE PRECISION` maps to `FloatingPoint(Double)`.
- `DECIMAL(p,s)` with `s > 0` maps to `Decimal(128)` with `precision = p`, `scale = s`, and `bit_width = 128`.
- Scale-zero `DECIMAL(p,0)` maps to signed `Int(32)` when `p <= 9`, signed `Int(64)` when `10 <= p <= 18`, and
  `Decimal(128)` with scale `0` when `p > 18`.
- Integer narrowing is allowed only when the complete declared precision range fits the selected signed width.
  Observed batch values cannot justify narrowing.
- Preserve the original type name, precision, and scale in field metadata.

The Exasol aliases `INTEGER` and `BIGINT`, where exposed by metadata, are represented using their resolved
`DECIMAL` precision and scale rather than treated as undocumented independent types.

For every decimal field, `0 <= scale <= precision` is required. Decimal conversion is parameter-preserving and
value-preserving within the declared range. Integer narrowing is value-preserving for the declared range but loses
the original decimal physical representation; the source metadata remains available for reconstruction.

Example field metadata (the physical type is `Decimal(128, precision=12, scale=2)`):

```json
{
  "name": "AMOUNT",
  "nullable": true,
  "arrow_storage_type": "Decimal(128)",
  "metadata": {
    "exasol:type_name": "DECIMAL(12,2)",
    "exasol:type_family": "DECIMAL",
    "exasol:precision": "12",
    "exasol:scale": "2"
  }
}
```

### Strings

`CHAR(n)` and `VARCHAR(n)` map to `Utf8`. Exasol permits `1 <= n <= 2000` for `CHAR(n)`, with default `n = 1`,
and `1 <= n <= 2,000,000` for `VARCHAR(n)`. Preserve the source type name, declared character length, and
`ASCII`/`UTF8` character set. `CHAR` padding remains an Exasol logical concern; it is not a reason to use
`FixedSizeBinary`, and the mapping does not reinterpret character data as bytes. An empty Exasol string is `NULL`
and therefore follows the nullable-field semantics.

### Date and time

- `DATE` maps to `Date(Day)`.
- `TIMESTAMP(p)` maps to `Timestamp(unit, "")`. Exasol permits `0 <= p <= 9`, with default `p = 3`.
- Timestamp unit selection is: `p = 0` seconds; `1 <= p <= 3` milliseconds; `4 <= p <= 6` microseconds; and
  `7 <= p <= 9` nanoseconds.
- `TIMESTAMP(p) WITH LOCAL TIME ZONE` is normalized to UTC using the session time zone and maps to
  `Timestamp(unit, "UTC")`. Preserve the original type name and precision in metadata.

UTC normalization is a semantic normalization, not lossless preservation of the original session-local
representation. Exasol internally stores these values normalized to UTC, while input and output are interpreted in
the session time zone. Timestamp precision is preserved by the unit selection. The Arrow timestamp unit is explicit
and its timezone is optional. See [Exasol data type details](https://docs.exasol.com/db/latest/sql_references/data_types/datatypedetails.htm)
and [the Arrow columnar format](https://arrow.apache.org/docs/format/Columnar.html).

Example UTC timestamp field:

```json
{
  "name": "CREATED_AT",
  "nullable": false,
  "arrow_storage_type": "Timestamp(Microsecond, UTC)",
  "metadata": {
    "exasol:type_name": "TIMESTAMP(6) WITH LOCAL TIME ZONE",
    "exasol:type_family": "TIMESTAMP WITH LOCAL TIME ZONE",
    "exasol:precision": "6"
  }
}
```

### HASHTYPE

`HASHTYPE(n BYTE)` maps to `FixedSizeBinary(byte_width = n)`. `HASHTYPE(m BIT)` maps to
`FixedSizeBinary(byte_width = m / 8)`. Exasol permits `1 <= n <= 1024` bytes, or `8 <= m <= 8192` bits, and `m`
must be a multiple of 8. The default is `HASHTYPE(16 BYTE)`. Invalid declarations are rejected.

Exasol accepts hexadecimal, UUID, Base64, and Base64URL strings as SQL input syntax; UUID input is supported only for
`HASHTYPE(16 BYTE)`. Transport raw hash bytes rather than any of those textual forms. Preserve the source type name
and canonical byte width in metadata. The `HASHTYPE_FORMAT` display setting, including UUID display, does not change
the Arrow type.

Example:

```json
{
  "name": "DIGEST",
  "nullable": true,
  "arrow_storage_type": "FixedSizeBinary(32)",
  "metadata": {
    "exasol:type_name": "HASHTYPE(256 BIT)",
    "exasol:type_family": "HASHTYPE",
    "exasol:hash_byte_width": "32"
  }
}
```

See [Exasol HASHTYPE documentation](https://docs.exasol.com/db/latest/sql_references/data_types/datatypedetails.htm?Highlight=hashtype).

### Intervals

`INTERVAL YEAR(p) TO MONTH` maps to an Arrow `Struct_` with two children, `years: Int(32, signed)` and
`months: Int(32, signed)`, annotated with `ARROW:extension:name = "exasol.interval.year_month"`. Encode the
normalized signed year and month components rather than a total-month integer. This preserves Exasol's complete
range, including `-999999999-11` through `999999999-11`. Preserve the declared leading-field precision in
`exasol:interval_leading_precision` and the complete SQL declaration in `exasol:type_name`.

The standard Arrow `YEAR_MONTH` interval is not sufficient for this mapping because it stores one signed 32-bit
total-month value. Exasol permits `999999999` years and `11` months, which requires
`999999999 * 12 + 11 = 11999999999` months, exceeding Arrow's signed 32-bit maximum of `2147483647`. The struct
extension therefore stores the year and month components separately, preserving the complete Exasol range without
overflow or loss of calendar semantics.

`INTERVAL DAY(lfp) TO SECOND(fsp)` maps to Arrow `Interval(MonthDayNano)`. Encode `months = 0`, the signed day count,
and the time-of-day component as signed nanoseconds. Preserve `lfp` in `exasol:interval_leading_precision`, `fsp`
in `exasol:interval_fractional_precision`, and the complete SQL declaration in `exasol:type_name`.

Exasol currently documents effective millisecond accuracy; encode those milliseconds as nanoseconds by multiplying by
`1,000,000`. If Exasol enables true `fsp = 9` storage, place the nanosecond value directly in the same Arrow type.
This represents the complete day range and future nanosecond precision. Do not convert either interval to a timestamp,
fixed-duration value, text, or generic binary; calendar interval semantics must remain explicit.

Example year-month interval field:

```json
{
  "name": "AGE",
  "nullable": true,
  "arrow_storage_type": "Struct<years: Int32, months: Int32>",
  "metadata": {
    "exasol:type_name": "INTERVAL YEAR(4) TO MONTH",
    "exasol:type_family": "INTERVAL YEAR TO MONTH",
    "exasol:interval_leading_precision": "4",
    "ARROW:extension:name": "exasol.interval.year_month",
    "ARROW:extension:metadata": "{\"layout\":\"years:int32,months:int32\"}"
  }
}
```

Example day-time interval field:

```json
{
  "name": "ELAPSED",
  "nullable": true,
  "arrow_storage_type": "Interval(MonthDayNano)",
  "metadata": {
    "exasol:type_name": "INTERVAL DAY(6) TO SECOND(3)",
    "exasol:type_family": "INTERVAL DAY TO SECOND",
    "exasol:interval_leading_precision": "6",
    "exasol:interval_fractional_precision": "3"
  }
}
```

### GEOMETRY

`GEOMETRY(srid)` uses variable-size Arrow `Binary` storage with GeoArrow's WKB extension. Exasol's supported
geometry objects are `POINT`, `LINESTRING`, `POLYGON`, `MULTIPOINT`, `MULTILINESTRING`, `MULTIPOLYGON`, and
`GEOMETRYCOLLECTION`. `srid` is optional and defaults to `0`, meaning no coordinate system:

- `ARROW:extension:name` is exactly `geoarrow.wkb`.
- `ARROW:extension:metadata` is UTF-8 JSON.
- For a present, nonzero SRID, the JSON object contains `"crs"` as the SRID text and `"crs_type"` as `"srid"`.
- For an absent or zero SRID, omit CRS metadata. Omit `edges` to select planar/linear edge semantics.
- Do not infer an EPSG authority from an Exasol SRID or force one geometry subtype: one column may contain the
  supported geometry object types listed above.
- Preserve the source type name and SRID in `exasol:*` metadata when needed for diagnostics or reconstruction.
- WKT may be accepted at the SQL boundary, but WKB is the canonical Arrow transport representation.

Example extension metadata:

```json
{
  "ARROW:extension:name": "geoarrow.wkb",
  "ARROW:extension:metadata": "{\"crs\":\"4326\",\"crs_type\":\"srid\"}",
  "exasol:type_name": "GEOMETRY(4326)",
  "exasol:type_family": "GEOMETRY",
  "exasol:srid": "4326"
}
```

See [GeoArrow extension types](https://geoarrow.org/extension-types.html) and [Exasol geometry documentation](https://docs.exasol.com/db/latest/sql_references/data_types/datatypedetails.htm).

## Official Exasol type surface

The complete Exasol SQL type surface considered by this mapping is:

`BOOLEAN`, `DECIMAL(p,s)`, `DOUBLE PRECISION`, `DATE`, `TIMESTAMP(p)`,
`TIMESTAMP(p) WITH LOCAL TIME ZONE`, `INTERVAL YEAR(p) TO MONTH`,
`INTERVAL DAY(lfp) TO SECOND(fsp)`, `GEOMETRY(srid)`, `HASHTYPE(n BYTE)`,
`HASHTYPE(m BIT)`, `CHAR(n)`, and `VARCHAR(n)`.

`INTERVAL YEAR(p) TO MONTH` uses leading-field precision `1 <= p <= 9`, default `p = 2`. `INTERVAL DAY(lfp) TO
SECOND(fsp)` uses leading-field precision `1 <= lfp <= 9` and fractional-second precision `0 <= fsp <= 9`, with
defaults `lfp = 2` and `fsp = 3`. Both interval types now have mappings above; the year-month struct mapping
preserves the complete declared range.

See the authoritative [Exasol data type overview](https://docs.exasol.com/db/latest/sql_references/data_types/datatypesoverview.htm)
and [data type details](https://docs.exasol.com/db/latest/sql_references/data_types/datatypedetails.htm).

## Unsupported declarations and future extensions

The following are unsupported in this version and must terminate schema conversion with a descriptive error:

- any Exasol type not represented by the current v2 schema;
- geometry encodings other than the defined WKB representation; and
- hash declarations with invalid or non-byte-aligned widths.

Unsupported declarations must not fall back to `Utf8` or generic `Binary` unless a future mapping explicitly defines the
required extension metadata. Future mappings may add Arrow interval types, native GeoArrow layouts such as
`geoarrow.point` or `geoarrow.polygon`, and additional Exasol-specific extension types.

## Round-trip classification

| Mapping | Classification | Reason |
| --- | --- | --- |
| `DOUBLE PRECISION`, `DATE`, `CHAR(n)`, `VARCHAR(n)`, `BOOLEAN` | Value-preserving | Source declaration metadata is retained. |
| Positive-scale `DECIMAL` | Parameter- and value-preserving | Decimal precision, scale, and bit width are explicit. |
| Scale-zero `DECIMAL` narrowed to Arrow `Int(32)` or `Int(64)` | Value-preserving, representation-normalized | The declared range fits; source parameters remain in metadata. |
| `TIMESTAMP(p)` | Precision- and value-preserving | The smallest sufficient Arrow unit is selected. |
| `TIMESTAMP ... WITH LOCAL TIME ZONE` | Value-preserving after UTC normalization | Original session-local representation is not preserved. |
| `HASHTYPE` | Byte-for-byte and width-preserving | Raw bytes and declared width are transported. |
| `GEOMETRY` | Geometry-value preserving, representation-normalized | WKT/engine representation becomes canonical WKB; SRID metadata is retained. |
| `INTERVAL YEAR(p) TO MONTH` | Range-preserving, extension-normalized | Struct fields preserve signed years and months across the complete Exasol range. |
| `INTERVAL DAY(lfp) TO SECOND(fsp)` | Range- and precision-preserving, representation-normalized | `MonthDayNano` stores zero months, days, and nanoseconds; current millisecond values are scaled to nanoseconds. |
| Unsupported type | Not representable | Schema conversion is rejected. |

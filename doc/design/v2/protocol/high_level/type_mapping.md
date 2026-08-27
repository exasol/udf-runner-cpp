# UDF Protocol v2: High-Level Exasol-to-Arrow Type Mapping

This document is the normative high-level type-conversion contract for v2 data-stream column schemas. The protocol uses a
self-owned subset of Arrow's schema model in `udf_protocol.fbs`; it does not import Arrow's FlatBuffers schema.
`Field.custom_metadata` carries only conversion-specific extension annotations as string key/value pairs.
The high-level column definition in `call_metadata` supplies the Exasol type family and all declared type parameters;
this document defines the corresponding Arrow storage representation and conversion-specific metadata.

The conversion is performed from the declared Exasol column type, never from values observed in a batch. A field's
`nullable` flag is preserved independently of its physical type. A conversion that is not defined here fails schema
conversion with a descriptive error.

The [mapping diagram](type_mapping.svg) summarizes the decision paths below.

## Official Exasol type mappings

`type` in v2 payload metadata identifies one of Exasol's official type families. `type_name` is the canonical,
complete SQL declaration, including parameters. Arrow types in the table are physical protocol representations, not
alternate Exasol type names.

| Exasol type family | Arrow-compatible representation | Contract |
| --- | --- | --- |
| `DOUBLE PRECISION` | `FloatingPoint(Double)` | Preserve nullable field semantics. |
| `DECIMAL` | `Decimal(32)`, `Decimal(64)`, or `Decimal(128)` | Select the smallest decimal width that preserves the declared precision and scale. |
| `TIMESTAMP` | `Timestamp(unit, "")` | Select the smallest unit preserving declared precision. |
| `TIMESTAMP WITH LOCAL TIME ZONE` | `Timestamp(unit, "UTC")` | Normalize Exasol's UTC-normalized value for transport. |
| `DATE` | `Date(Day)` | Preserve calendar-day semantics. |
| `CHAR` | `Utf8` | Preserve fixed-length padding and character metadata as Exasol semantics. |
| `VARCHAR` | `Utf8` | Preserve declared character length and character metadata. |
| `BOOLEAN` | `Bool` | Preserve nullable values. |
| `HASHTYPE` | `FixedSizeBinary` | Preserve declared byte width and transport raw bytes. |
| `GEOMETRY` | `Binary` + `geoarrow.wkb` | Transport WKB and preserve SRID metadata. |
| `INTERVAL YEAR TO MONTH` | Signed `Int32` or `Int64` total-month count + `exasol.interval.year_month` | Use Int32 for precisions 1–8 and Int64 for precision 9. |
| `INTERVAL DAY TO SECOND` | `Interval(MonthDayNano)` | Encode zero months, signed days, and nanoseconds. |

The column properties and their API meaning are defined by the [Call Metadata contract](payloads.md#call-metadata).

The official Exasol documentation defines each type's SQL syntax, aliases, parameter limits, and default values. This
document defines only the base type-family to Arrow mapping and the conversion-critical parameter handling. See the
[Exasol data type overview](https://docs.exasol.com/db/latest/sql_references/data_types/datatypesoverview.htm) and
[Exasol data type details](https://docs.exasol.com/db/latest/sql_references/data_types/datatypedetails.htm) for the
complete SQL type definitions.

### Legacy v1 category replacement

The v1 protocol used `DOUBLE`, `INT32`, `INT64`, `NUMERIC`, `TIMESTAMP`, and `STRING` as internal categories, and
`UNSUPPORTED` as an error sentinel. They are not Exasol SQL types and are not valid v2 `type` values. Their v2
replacements are `DOUBLE PRECISION`, `DECIMAL`, `TIMESTAMP` or `TIMESTAMP WITH LOCAL TIME ZONE`, and `CHAR` or
`VARCHAR`. `INT32` and `INT64` may still appear as Arrow physical storage choices for other mappings; they are not
the canonical representation of SQL `DECIMAL`. `UNSUPPORTED` is a conversion failure, never a type.

## Concrete Exasol SQL mappings

### Numeric values

For a declared `DECIMAL(p,s)`, select the smallest Arrow decimal width that supports the declared precision.

- `DOUBLE PRECISION` maps to `FloatingPoint(Double)`.
- `1 <= p <= 9` maps to `Decimal(32)` with `bit_width = 32`.
- `10 <= p <= 18` maps to `Decimal(64)` with `bit_width = 64`.
- `19 <= p <= 36` maps to `Decimal(128)` with `bit_width = 128`.
- All three mappings preserve `precision = p` and `scale = s`, including scale-zero decimals. Consumers may cast a
  decimal to an integer when appropriate, but observed values never change the protocol representation.
For every decimal field, `0 <= scale <= precision` is required. Decimal conversion is parameter-preserving and
value-preserving within the declared range. The selected decimal width is the smallest width supported by Arrow for
the declared precision.

Example field metadata (the physical type is `Decimal(64, precision=12, scale=2)`):

See the [decimal field metadata example](examples/decimal_field_metadata.json).

### Strings

`CHAR(n)` and `VARCHAR(n)` map to `Utf8`. Preserve the declared character length and `ASCII`/`UTF8` character set.
The official Exasol documentation defines the valid lengths and character-set syntax.
`CHAR` padding remains an Exasol logical concern; it is not a reason to use
`FixedSizeBinary`, and the mapping does not reinterpret character data as bytes. An empty Exasol string is `NULL`
and therefore follows the nullable-field semantics.

### Date and time

- `DATE` maps to `Date(Day)`.
- `TIMESTAMP(p)` maps to `Timestamp(unit, "")`; the declared fractional precision selects the unit.
- Timestamp unit selection is: `p = 0` seconds; `1 <= p <= 3` milliseconds; `4 <= p <= 6` microseconds; and
  `7 <= p <= 9` nanoseconds.
- `TIMESTAMP(p) WITH LOCAL TIME ZONE` is normalized to UTC using the session time zone and maps to
  `Timestamp(unit, "UTC")`.

UTC normalization is a semantic normalization, not lossless preservation of the original session-local
representation. Exasol internally stores these values normalized to UTC, while input and output are interpreted in
the session time zone. Timestamp precision is preserved by the unit selection. The Arrow timestamp unit is explicit
and its timezone is optional. See [Exasol data type details](https://docs.exasol.com/db/latest/sql_references/data_types/datatypedetails.htm)
and [the Arrow columnar format](https://arrow.apache.org/docs/format/Columnar.html).

Example UTC timestamp field:

See the [timestamp field metadata example](examples/timestamp_field_metadata.json).

### HASHTYPE

`HASHTYPE(n BYTE)` maps to `FixedSizeBinary(byte_width = n)`. `HASHTYPE(m BIT)` maps to
`FixedSizeBinary(byte_width = m / 8)`. The declared unit is supplied through `size_unit`; bit declarations must be
byte-aligned and invalid declarations are rejected. The official Exasol documentation defines the valid sizes and
input/display formats.

Exasol accepts hexadecimal, UUID, Base64, and Base64URL strings as SQL input syntax; UUID input is supported only for
`HASHTYPE(16 BYTE)`. Transport raw hash bytes rather than any of those textual forms. The `HASHTYPE_FORMAT` display
setting, including UUID display, does not change the Arrow type.

Example:

See the [HASHTYPE field metadata example](examples/hashtype_field_metadata.json).

See [Exasol HASHTYPE documentation](https://docs.exasol.com/db/latest/sql_references/data_types/datatypedetails.htm?Highlight=hashtype).

### Intervals

`INTERVAL YEAR(p) TO MONTH` maps to a signed integer containing the normalized total number of months, annotated with
`ARROW:extension:name = "exasol.interval.year_month"`. Encode `years * 12 + months`; the integer remains signed so
negative intervals use negative month counts. Use signed `Int32` for declared year precisions 1 through 8 and signed
`Int64` for precision 9:

| Year precision | Maximum absolute month count | Arrow storage |
| --- | ---: | --- |
| `p = 1–8` | `119` to `1,199,999,999` | `Int32` |
| `p = 9` | `11,999,999,999` | `Int64` |

The maximum supported declaration, `999999999` years and `11` months, requires
`999999999 * 12 + 11 = 11999999999` months. It therefore requires signed `Int64`; precisions 1 through 8 use signed
`Int32` storage. The extension metadata describes the logical signed-total-month layout, while the Arrow field type
defines the physical width.

`INTERVAL DAY(lfp) TO SECOND(fsp)` maps to Arrow `Interval(MonthDayNano)`. Encode `months = 0`, the signed day count,
and the time-of-day component as signed nanoseconds.

Encode the source fractional seconds as nanoseconds. If the source accuracy is milliseconds, multiply by
`1,000,000`; if nanosecond accuracy is available, place the nanosecond value directly in the same Arrow type.
This represents the complete day range and future nanosecond precision. Do not convert either interval to a timestamp,
fixed-duration value, text, or generic binary; calendar interval semantics must remain explicit.

Example year-month interval field:

See the [year-month interval field metadata example](examples/year_month_interval_field_metadata.json).

Example day-time interval field:

See the [day-time interval field metadata example](examples/day_time_interval_field_metadata.json).

### GEOMETRY

`GEOMETRY(srid)` uses variable-size Arrow `Binary` storage with GeoArrow's WKB extension. Exasol's supported
geometry objects are `POINT`, `LINESTRING`, `POLYGON`, `MULTIPOINT`, `MULTILINESTRING`, `MULTIPOLYGON`, and
`GEOMETRYCOLLECTION`.

- `ARROW:extension:name` is exactly `geoarrow.wkb`.
- `ARROW:extension:metadata` is UTF-8 JSON.
- For a present, nonzero SRID, the JSON object contains `"crs"` as the SRID text and `"crs_type"` as `"srid"`.
- For an absent or zero SRID, omit CRS metadata. Omit `edges` to select planar/linear edge semantics.
- Do not infer an EPSG authority from an Exasol SRID or force one geometry subtype: one column may contain the
  supported geometry object types listed above.
- WKT may be accepted at the SQL boundary, but WKB is the canonical Arrow transport representation.

Example extension metadata:

See the [GeoArrow extension metadata example](examples/geometry_extension_metadata.json).

See [GeoArrow extension types](https://geoarrow.org/extension-types.html) and [Exasol geometry documentation](https://docs.exasol.com/db/latest/sql_references/data_types/datatypedetails.htm).

## Official Exasol type surface

The complete Exasol SQL type surface considered by this mapping is:

`BOOLEAN`, `DECIMAL(p,s)`, `DOUBLE PRECISION`, `DATE`, `TIMESTAMP(p)`,
`TIMESTAMP(p) WITH LOCAL TIME ZONE`, `INTERVAL YEAR(p) TO MONTH`,
`INTERVAL DAY(lfp) TO SECOND(fsp)`, `GEOMETRY(srid)`, `HASHTYPE(n BYTE)`,
`HASHTYPE(m BIT)`, `CHAR(n)`, and `VARCHAR(n)`.

Both interval mappings preserve the declared leading and fractional-second precision supplied by the dedicated column
properties. The official Exasol documentation defines the valid precision ranges and omitted-parameter behavior.

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
| `DOUBLE PRECISION`, `DATE`, `CHAR(n)`, `VARCHAR(n)`, `BOOLEAN` | Value-preserving | The Arrow representation preserves the values and declared semantics. |
| `DECIMAL` | Parameter- and value-preserving | The smallest Decimal32/64/128 width is selected from declared precision and scale. |
| `TIMESTAMP(p)` | Precision- and value-preserving | The smallest sufficient Arrow unit is selected. |
| `TIMESTAMP ... WITH LOCAL TIME ZONE` | Value-preserving after UTC normalization | Original session-local representation is not preserved. |
| `HASHTYPE` | Byte-for-byte and width-preserving | Raw bytes and declared width are transported. |
| `GEOMETRY` | Geometry-value preserving, representation-normalized | WKT/engine representation becomes canonical WKB; SRID metadata is retained. |
| `INTERVAL YEAR(p) TO MONTH` | Range-preserving, extension-normalized | A precision-appropriate signed integer preserves the normalized total-month value across the complete Exasol range. |
| `INTERVAL DAY(lfp) TO SECOND(fsp)` | Range- and precision-preserving, representation-normalized | `MonthDayNano` stores zero months, days, and nanoseconds; current millisecond values are scaled to nanoseconds. |
| Unsupported type | Not representable | Schema conversion is rejected. |

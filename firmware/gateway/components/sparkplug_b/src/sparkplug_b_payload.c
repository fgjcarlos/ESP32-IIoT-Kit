// Sparkplug B payload encode/decode implementation.
//
// Uses nanopb 0.4.x's callback-based encoding API over the vendored
// sparkplug_b.pb.{c,h} generated bindings.
//
// Why callbacks: with the vendored Tahu schema, setting
// `Payload.metrics type:FT_STATIC` cascades Metric's optional fields
// (alias, datatype, value, etc.) to callbacks due to a nanopb heuristic
// that prefers callbacks when a field uses `oneof`. The container
// (Payload.metrics) is still static (bounded to 16 metrics per .options),
// so we know exactly how many slots to allocate, but each individual
// metric value is filled via the field callbacks below.
//
// We define small wrapper callbacks per scalar type that emit the
// protobuf tag (via the field descriptor nanopb passes in) and the value
// (via the low-level pb_encode_* helpers). Each callback reads its
// argument as a pointer to the typed value.

#include "sparkplug_b_payload.h"

#include <string.h>

#include "esp_log.h"
#include "pb_decode.h"
#include "pb_encode.h"

#include "sparkplug_b.pb.h"
#include "sparkplug_b_state.h"

#define TAG "sparkplug_b_payload"

// Cap at 16 metrics to match the nanopb option
// `org.eclipse.tahu.protobuf.Payload.metrics max_count:16`.
#define SPARKPLUG_B_METRICS_MAX 16

// Static decode buffer. Lives for the lifetime of the program; the gateway
// never runs in a multi-threaded context relative to this module.
static org_eclipse_tahu_protobuf_Payload s_payload = {0};

// ---------------------------------------------------------------------------
// Encode wrappers
//
// Each callback writes (tag, value) into the output stream. The field tag
// is provided by nanopb via `field->tag` (wire type + field number
// already encoded in the varint format).
// ---------------------------------------------------------------------------

static bool enc_u64(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    uint64_t value = *(const uint64_t *)(*arg);
    if (!pb_encode_tag(stream, PB_WT_VARINT, field->tag)) {
        return false;
    }
    return pb_encode_varint(stream, value);
}

static bool enc_u32(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    uint32_t value = *(const uint32_t *)(*arg);
    if (!pb_encode_tag(stream, PB_WT_VARINT, field->tag)) {
        return false;
    }
    return pb_encode_varint(stream, value);
}

static bool enc_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    const char *str = (const char *)(*arg);
    if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
        return false;
    }
    return pb_encode_string(stream, (const pb_byte_t *)str, strlen(str));
}

// The oneof value arms of Payload.Metric all live in the same C union, so
// they share one address and only one callback can be registered at a time.
// The callback therefore dispatches on the field tag, which identifies the
// arm nanopb is currently encoding or decoding.
static bool enc_value(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    const sparkplug_metric_t *metric = (const sparkplug_metric_t *)(*arg);

    switch (field->tag) {
    case org_eclipse_tahu_protobuf_Payload_Metric_int_value_tag:
        if (!pb_encode_tag(stream, PB_WT_VARINT, field->tag)) {
            return false;
        }
        // Int8 and UInt8 both travel in the int_value arm; the Int8 keeps its
        // two's complement bit pattern, widened to 32 bits.
        if (metric->data_type == SPARKPLUG_DATA_TYPE_INT8) {
            return pb_encode_varint(stream, (uint64_t)(uint32_t)(int32_t)metric->value.int8_value);
        }
        return pb_encode_varint(stream, metric->value.uint8_value);
    case org_eclipse_tahu_protobuf_Payload_Metric_long_value_tag:
        if (!pb_encode_tag(stream, PB_WT_VARINT, field->tag)) {
            return false;
        }
        return pb_encode_varint(stream, (uint64_t)metric->value.int64_value);
    case org_eclipse_tahu_protobuf_Payload_Metric_float_value_tag:
        if (!pb_encode_tag(stream, PB_WT_32BIT, field->tag)) {
            return false;
        }
        return pb_encode_fixed32(stream, &metric->value.float_value);
    case org_eclipse_tahu_protobuf_Payload_Metric_double_value_tag:
        if (!pb_encode_tag(stream, PB_WT_64BIT, field->tag)) {
            return false;
        }
        return pb_encode_fixed64(stream, &metric->value.double_value);
    case org_eclipse_tahu_protobuf_Payload_Metric_boolean_value_tag:
        if (!pb_encode_tag(stream, PB_WT_VARINT, field->tag)) {
            return false;
        }
        return pb_encode_varint(stream, metric->value.boolean_value ? 1U : 0U);
    default:
        // An arm this gateway does not model (bytes, dataset, template):
        // skip it rather than encoding a wrong value.
        return true;
    }
}

// Per-metric storage for the datatype. The generated Metric struct keeps the
// datatype in a callback, so each metric slot needs its own value slot: a
// single shared static would make every metric carry the datatype of the
// last metric that was prepared.
static uint32_t s_datatype[SPARKPLUG_B_METRICS_MAX];

static void init_metric_callbacks(org_eclipse_tahu_protobuf_Payload_Metric *m,
                                  const sparkplug_metric_t *src,
                                  size_t index)
{
    memset(m, 0, sizeof(*m));

    // Alias: required by Sparkplug spec for DDATA; optional on NBIRTH (name is
    // enough) but harmless to always send on NBIRTH for diagnostic purposes.
    // SPARKPLUG_ALIAS_NONE suppresses the field entirely, which the spec
    // requires for bdSeq and Node Control/Rebirth.
    if (src->alias != SPARKPLUG_ALIAS_NONE) {
        m->alias.funcs.encode = enc_u64;
        m->alias.arg = (void *)&src->alias;
    }

    // Name: only included when caller provides one. Spec §6.4.3 says names
    // are required on NBIRTH/DBIRTH and stripped on DDATA.
    if (src->name != NULL) {
        m->name.funcs.encode = enc_string;
        m->name.arg = (void *)src->name;
    }

    // Datatype: required by spec on NBIRTH, optional on DDATA. Always
    // include it because the broker uses it to decode values regardless
    // of whether the field is repeated.
    s_datatype[index] = (uint32_t)src->data_type;
    m->datatype.funcs.encode = enc_u32;
    m->datatype.arg = &s_datatype[index];

    // Value (oneof-style: pick the matching union arm). Only one callback can
    // live in the shared union, so enc_value dispatches on the arm tag.
    switch (src->data_type) {
    case SPARKPLUG_DATA_TYPE_DOUBLE:
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_double_value_tag;
        break;
    case SPARKPLUG_DATA_TYPE_FLOAT:
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_float_value_tag;
        break;
    case SPARKPLUG_DATA_TYPE_INT64:
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_long_value_tag;
        break;
    case SPARKPLUG_DATA_TYPE_BOOLEAN:
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_boolean_value_tag;
        break;
    case SPARKPLUG_DATA_TYPE_INT8: {
        // Sparkplug encodes Int8 as `int_value` (uint32 on the wire). The
        // bit pattern is preserved, so the value is widened on the way out.
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_int_value_tag;
        break;
    }
    case SPARKPLUG_DATA_TYPE_UINT8: {
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_int_value_tag;
        break;
    }
    default:
        // Unsupported type — leave which_value unset; nanopb will skip it.
        return;
    }

    m->value.double_value.funcs.encode = enc_value;
    m->value.double_value.arg = (void *)src;
}

esp_err_t sparkplug_b_encode_payload(const sparkplug_payload_t *payload,
                                     uint8_t *out_buf, size_t out_buf_len,
                                     size_t *out_len)
{
    if (payload == NULL || out_buf == NULL || out_len == NULL ||
        payload->metric_count == 0 ||
        payload->metric_count > SPARKPLUG_B_METRICS_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (out_buf_len < SPARKPLUG_B_PAYLOAD_MAX_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }

    org_eclipse_tahu_protobuf_Payload pb = {0};
    pb.has_timestamp = true;
    pb.timestamp = payload->timestamp_ms;
    pb.has_seq = true;
    pb.seq = payload->seq;

    pb.metrics_count = (pb_size_t)payload->metric_count;
    for (size_t i = 0; i < payload->metric_count; ++i) {
        init_metric_callbacks(&pb.metrics[i], &payload->metrics[i], i);
    }

    pb_ostream_t stream = pb_ostream_from_buffer(out_buf, (unsigned)out_buf_len);
    if (!pb_encode(&stream,
                   org_eclipse_tahu_protobuf_Payload_fields,
                   &pb)) {
        ESP_LOGE(TAG, "pb_encode failed: %s", PB_GET_ERROR(&stream));
        return ESP_ERR_INVALID_SIZE;
    }

    *out_len = (size_t)stream.bytes_written;
    return ESP_OK;
}

esp_err_t sparkplug_b_encode_nbirth(uint8_t *out_buf, size_t out_buf_len,
                                    uint64_t bd_seq, uint64_t uptime_ms,
                                    size_t *out_len)
{
    sparkplug_metric_t metrics[3] = {
        // bdSeq is never aliased so a Host Application can match the death
        // certificate with the birth on its own.
        { .alias = SPARKPLUG_ALIAS_NONE, .name = SPARKPLUG_METRIC_BDSEQ_NAME,
          .data_type = SPARKPLUG_DATA_TYPE_INT64,
          .value.int64_value = (int64_t)bd_seq },
        // Sparkplug B v3.0.0 requires this metric in every NBIRTH, and it
        // must not carry an alias.
        { .alias = SPARKPLUG_ALIAS_NONE, .name = SPARKPLUG_METRIC_REBIRTH_NAME,
          .data_type = SPARKPLUG_DATA_TYPE_BOOLEAN,
          .value.boolean_value = false },
        { .alias = SPARKPLUG_ALIAS_UPTIME_MS, .name = "Inputs/UptimeMs",
          .data_type = SPARKPLUG_DATA_TYPE_INT64, .value.int64_value = (int64_t)uptime_ms },
    };
    sparkplug_payload_t payload = {
        .timestamp_ms = uptime_ms,
        // The birth opens the sequence of the session, so it carries seq 0.
        .seq = sparkplug_b_seq_reset(),
        .metrics = metrics,
        .metric_count = 3,
    };
    return sparkplug_b_encode_payload(&payload, out_buf, out_buf_len, out_len);
}

esp_err_t sparkplug_b_encode_death(uint8_t *out_buf, size_t out_buf_len,
                                    uint64_t bd_seq, uint64_t timestamp_ms,
                                    size_t *out_len)
{
    if (out_buf == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (out_buf_len < SPARKPLUG_B_PAYLOAD_MAX_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }

    sparkplug_metric_t metric = {
        // The bdSeq metric is never aliased: the Host Application identifies
        // it by name so the death certificate stays readable on its own.
        .alias = SPARKPLUG_ALIAS_NONE,
        .name = SPARKPLUG_METRIC_BDSEQ_NAME,
        .data_type = SPARKPLUG_DATA_TYPE_INT64,
        .value.int64_value = (int64_t)bd_seq,
    };

    org_eclipse_tahu_protobuf_Payload pb = {0};
    pb.has_timestamp = true;
    pb.timestamp = timestamp_ms;
    // No sequence number: the death certificate is published by the broker on
    // behalf of the Edge Node, which has no sequence to report.
    pb.has_seq = false;
    pb.metrics_count = 1;
    init_metric_callbacks(&pb.metrics[0], &metric, 0);

    pb_ostream_t stream = pb_ostream_from_buffer(out_buf, (unsigned)out_buf_len);
    if (!pb_encode(&stream,
                   org_eclipse_tahu_protobuf_Payload_fields,
                   &pb)) {
        ESP_LOGE(TAG, "pb_encode failed: %s", PB_GET_ERROR(&stream));
        return ESP_ERR_INVALID_SIZE;
    }

    *out_len = (size_t)stream.bytes_written;
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Decode path
//
// The vendored Tahu schema declares every Payload.Metric field as a nanopb
// callback, so a plain pb_decode() silently skips the metric contents: the
// metric *count* survives, but names, aliases and values do not. Command
// handling (NCMD/DCMD) needs the metric name and value, so the decoder
// installs per-field decode callbacks before decoding.
//
// nanopb 0.4 callback contract (see managed_components/livekit__nanopb/src/pb_decode.c):
//   - length-delimited (string) fields: the callback receives a substream
//     holding only the string bytes; the length prefix is already consumed.
//   - scalar fields: the callback receives a substream holding the raw
//     varint/fixed bytes, which must be decoded with pb_decode_varint() etc.
// ---------------------------------------------------------------------------

// Scratch space for the decoded metrics. The gateway decodes commands from
// the single MQTT event task, so a static buffer is enough; keeping it static
// also gives the nanopb callback arguments a stable address.
static sparkplug_b_decoded_metric_t s_decoded[SPARKPLUG_B_MAX_DECODED_METRICS];

static bool dec_name(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    sparkplug_b_decoded_metric_t *metric = (sparkplug_b_decoded_metric_t *)(*arg);
    size_t len = stream->bytes_left;

    if (len >= SPARKPLUG_B_METRIC_NAME_MAX) {
        len = SPARKPLUG_B_METRIC_NAME_MAX - 1;
    }
    if (len > 0 && !pb_read(stream, (pb_byte_t *)metric->name, len)) {
        return false;
    }
    metric->name[len] = '\0';

    // Drain the bytes dropped by truncation: nanopb requires the substream to
    // be fully consumed or it reports a decode error.
    while (stream->bytes_left > 0) {
        pb_byte_t discard;
        if (!pb_read(stream, &discard, 1)) {
            return false;
        }
    }
    return true;
}

static bool dec_alias(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    sparkplug_b_decoded_metric_t *metric = (sparkplug_b_decoded_metric_t *)(*arg);
    uint64_t value = 0;

    if (!pb_decode_varint(stream, &value)) {
        return false;
    }
    metric->alias = value;
    metric->has_alias = true;
    return true;
}

static bool dec_datatype(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    sparkplug_b_decoded_metric_t *metric = (sparkplug_b_decoded_metric_t *)(*arg);
    uint32_t value = 0;

    if (!pb_decode_varint32(stream, &value)) {
        return false;
    }
    metric->datatype = value;
    return true;
}

// The oneof value arms share one C union, so only one callback can be
// registered. The callback dispatches on the field tag, which identifies the
// arm nanopb is decoding.
static bool dec_value(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    sparkplug_b_decoded_metric_t *metric = (sparkplug_b_decoded_metric_t *)(*arg);

    switch (field->tag) {
    case org_eclipse_tahu_protobuf_Payload_Metric_int_value_tag: {
        uint32_t value = 0;
        if (!pb_decode_varint32(stream, &value)) {
            return false;
        }
        metric->value.int_value = value;
        break;
    }
    case org_eclipse_tahu_protobuf_Payload_Metric_long_value_tag: {
        uint64_t value = 0;
        // Sparkplug encodes int64 as a plain (non-zigzag) varint, so read it
        // as an unsigned varint and reinterpret the bits.
        if (!pb_decode_varint(stream, &value)) {
            return false;
        }
        metric->value.long_value = (int64_t)value;
        break;
    }
    case org_eclipse_tahu_protobuf_Payload_Metric_float_value_tag: {
        float value = 0.0f;
        if (!pb_decode_fixed32(stream, &value)) {
            return false;
        }
        metric->value.float_value = value;
        break;
    }
    case org_eclipse_tahu_protobuf_Payload_Metric_double_value_tag: {
        double value = 0.0;
        if (!pb_decode_fixed64(stream, &value)) {
            return false;
        }
        metric->value.double_value = value;
        break;
    }
    case org_eclipse_tahu_protobuf_Payload_Metric_boolean_value_tag: {
        bool value = false;
        if (!pb_decode_bool(stream, &value)) {
            return false;
        }
        metric->value.boolean_value = value;
        break;
    }
    case org_eclipse_tahu_protobuf_Payload_Metric_string_value_tag: {
        size_t len = stream->bytes_left;
        if (len >= SPARKPLUG_B_METRIC_NAME_MAX) {
            len = SPARKPLUG_B_METRIC_NAME_MAX - 1;
        }
        if (len > 0 && !pb_read(stream, (pb_byte_t *)metric->value.string_value, len)) {
            return false;
        }
        metric->value.string_value[len] = '\0';
        while (stream->bytes_left > 0) {
            pb_byte_t discard;
            if (!pb_read(stream, &discard, 1)) {
                return false;
            }
        }
        break;
    }
    default:
        // An arm this gateway does not model: consume it and ignore it.
        while (stream->bytes_left > 0) {
            pb_byte_t discard;
            if (!pb_read(stream, &discard, 1)) {
                return false;
            }
        }
        return true;
    }

    metric->has_value = true;
    return true;
}

// Install the decode callbacks on one Metric slot.
//
// The oneof value arms share a union, so nanopb zeroes that union whenever a
// different arm has already been decoded for the same metric. A Sparkplug
// metric carries at most one value arm on the wire, so the zeroing can only
// ever happen once per metric and never clobbers the arm being decoded.
static void init_metric_decode_callbacks(org_eclipse_tahu_protobuf_Payload_Metric *m,
                                         sparkplug_b_decoded_metric_t *sink)
{
    memset(m, 0, sizeof(*m));

    m->name.funcs.decode = dec_name;
    m->name.arg = sink;

    m->alias.funcs.decode = dec_alias;
    m->alias.arg = sink;

    m->datatype.funcs.decode = dec_datatype;
    m->datatype.arg = sink;

    // One callback for the whole oneof: the arms share their storage.
    m->value.double_value.funcs.decode = dec_value;
    m->value.double_value.arg = sink;
}

esp_err_t sparkplug_b_decode_payload(const uint8_t *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_payload, 0, sizeof(s_payload));

    pb_istream_t stream = pb_istream_from_buffer(buf, (unsigned)len);
    if (!pb_decode(&stream,
                   org_eclipse_tahu_protobuf_Payload_fields,
                   &s_payload)) {
        ESP_LOGE(TAG, "pb_decode failed: %s", PB_GET_ERROR(&stream));
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

size_t sparkplug_b_payload_metric_count(void)
{
    return (size_t)s_payload.metrics_count;
}

esp_err_t sparkplug_b_decode_metrics(const uint8_t *buf, size_t len,                                     sparkplug_b_decoded_metric_t *out,
                                     size_t max_count,
                                     size_t *out_count)
{
    if (buf == NULL || len == 0 || out == NULL || out_count == NULL || max_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_decoded, 0, sizeof(s_decoded));
    *out_count = 0;

    org_eclipse_tahu_protobuf_Payload pb = {0};
    for (size_t i = 0; i < SPARKPLUG_B_METRICS_MAX; ++i) {
        init_metric_decode_callbacks(&pb.metrics[i], &s_decoded[i]);
    }

    pb_istream_t stream = pb_istream_from_buffer(buf, (unsigned)len);
    if (!pb_decode(&stream,
                   org_eclipse_tahu_protobuf_Payload_fields,
                   &pb)) {
        ESP_LOGW(TAG, "pb_decode failed: %s", PB_GET_ERROR(&stream));
        return ESP_ERR_INVALID_SIZE;
    }

    size_t decoded = (size_t)pb.metrics_count;
    size_t copied = decoded < max_count ? decoded : max_count;
    memcpy(out, s_decoded, copied * sizeof(*out));
    *out_count = decoded;

    return ESP_OK;
}
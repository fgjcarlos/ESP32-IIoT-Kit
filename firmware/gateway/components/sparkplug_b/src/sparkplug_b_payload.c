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

static bool enc_double(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    double value = *(const double *)(*arg);
    if (!pb_encode_tag(stream, PB_WT_64BIT, field->tag)) {
        return false;
    }
    return pb_encode_fixed64(stream, &value);
}

static bool enc_float(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    float value = *(const float *)(*arg);
    if (!pb_encode_tag(stream, PB_WT_32BIT, field->tag)) {
        return false;
    }
    return pb_encode_fixed32(stream, &value);
}

static void init_metric_callbacks(org_eclipse_tahu_protobuf_Payload_Metric *m,
                                  const sparkplug_metric_t *src)
{
    memset(m, 0, sizeof(*m));

    // Alias: required by Sparkplug spec for DDATA; optional on NBIRTH (name is
    // enough) but harmless to always send on NBIRTH for diagnostic purposes.
    m->alias.funcs.encode = enc_u64;
    m->alias.arg = (void *)&src->alias;

    // Name: only included when caller provides one. Spec §6.4.3 says names
    // are required on NBIRTH/DBIRTH and stripped on DDATA.
    if (src->name != NULL) {
        m->name.funcs.encode = enc_string;
        m->name.arg = (void *)src->name;
    }

    // Datatype: required by spec on NBIRTH, optional on DDATA. Always
    // include it because the broker uses it to decode values regardless
    // of whether the field is repeated.
    static uint32_t s_dt;
    s_dt = (uint32_t)src->data_type;
    m->datatype.funcs.encode = enc_u32;
    m->datatype.arg = &s_dt;

    // Value (oneof-style: pick the matching union arm).
    switch (src->data_type) {
    case SPARKPLUG_DATA_TYPE_DOUBLE:
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_double_value_tag;
        m->value.double_value.funcs.encode = enc_double;
        m->value.double_value.arg = (void *)&src->value.double_value;
        break;
    case SPARKPLUG_DATA_TYPE_FLOAT:
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_float_value_tag;
        m->value.float_value.funcs.encode = enc_float;
        m->value.float_value.arg = (void *)&src->value.float_value;
        break;
    case SPARKPLUG_DATA_TYPE_INT64:
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_long_value_tag;
        m->value.long_value.funcs.encode = enc_u64;
        m->value.long_value.arg = (void *)&src->value.int64_value;
        break;
    case SPARKPLUG_DATA_TYPE_INT8: {
        // Sparkplug encodes Int8 as `int_value` (uint32 on the wire). The
        // bit pattern is preserved, so we cast through a static copy.
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_int_value_tag;
        static uint32_t s_int_storage;
        s_int_storage = (uint32_t)(int32_t)src->value.int8_value;
        m->value.int_value.funcs.encode = enc_u32;
        m->value.int_value.arg = &s_int_storage;
        break;
    }
    case SPARKPLUG_DATA_TYPE_UINT8: {
        m->which_value = org_eclipse_tahu_protobuf_Payload_Metric_int_value_tag;
        static uint32_t s_uint_storage;
        s_uint_storage = (uint32_t)src->value.uint8_value;
        m->value.int_value.funcs.encode = enc_u32;
        m->value.int_value.arg = &s_uint_storage;
        break;
    }
    default:
        // Unsupported type — leave which_value unset; nanopb will skip it.
        break;
    }
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
        init_metric_callbacks(&pb.metrics[i], &payload->metrics[i]);
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

// ---------------------------------------------------------------------------
// Decode path (commit 4: structural only)
// ---------------------------------------------------------------------------

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
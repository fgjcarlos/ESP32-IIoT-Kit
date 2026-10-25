// Stress harness for the gateway's ESP-NOW receive path (issue #14).
//
// The gateway has to hold several nodes at the same time: every node behind
// it is a Sparkplug Device, and the registry, the sequence space and the
// metric sets are all shared state. A bug that only shows up with ten nodes
// (a registry that overwrites an entry, a sequence that leaks between nodes,
// a device id that belongs to the wrong node) is invisible to a unit test
// with one node.
//
// This harness drives the REAL bridge, the REAL protocol codec and the REAL
// device registry with N synthetic nodes and measures what a soak test on the
// board would measure:
//
//   - how many frames the gateway accepted;
//   - whether every node kept its own identity (device id and MAC);
//   - whether the sequence space advanced once per published message;
//   - whether the registry grew with the number of nodes;
//   - how much resident memory the process needed.
//
// The radio is not part of this harness: the frames are injected at the same
// point where the ESP-NOW receive callback injects them. The plan of the
// stress test with real nodes and real radio is docs/testing/stress-plan.md.
//
// Usage:
//   iot_stress [--nodes N] [--rounds M] [--json FILE]
//
// Exit code 0 when every acceptance criterion is met, 1 otherwise.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"
#include "sparkplug_b_device.h"
#include "sparkplug_b_espnow_bridge.h"
#include "sparkplug_b_state.h"

#define DEFAULT_NODES 10
#define DEFAULT_ROUNDS 20
#define MAX_NODES 64

// The acceptance criteria of the stress plan (docs/testing/stress-plan.md).
#define MAX_ACCEPTED_LOSS_PERCENT 0
#define MAX_RSS_GROWTH_KIB 8192

static uint8_t node_mac(unsigned int index, uint8_t out[6])
{
    // The device id of a node is built from the last two bytes of its MAC, so
    // the last two bytes have to be unique for the nodes to be distinct.
    out[0] = 0x02;
    out[1] = 0x00;
    out[2] = 0x00;
    out[3] = 0x00;
    out[4] = (uint8_t)((index >> 8) & 0xFF);
    out[5] = (uint8_t)(index & 0xFF);
    return out[6];
}

/** Resident memory of the process, in KiB. Zero when it cannot be read. */
static long rss_kib(void)
{
    FILE *statm = fopen("/proc/self/statm", "r");
    if (statm == NULL) {
        return 0;
    }

    long total = 0;
    long resident = 0;
    if (fscanf(statm, "%ld %ld", &total, &resident) != 2) {
        fclose(statm);
        return 0;
    }
    fclose(statm);

    // The second field of /proc/self/statm is the resident set in PAGES.
    const long page_kib = 4;
    return resident * page_kib;
}

int main(int argc, char **argv)
{
    unsigned int nodes = DEFAULT_NODES;
    unsigned int rounds = DEFAULT_ROUNDS;
    const char *json_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--nodes") == 0 && i + 1 < argc) {
            nodes = (unsigned int)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--rounds") == 0 && i + 1 < argc) {
            rounds = (unsigned int)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--json") == 0 && i + 1 < argc) {
            json_path = argv[++i];
        } else {
            fprintf(stderr, "usage: %s [--nodes N] [--rounds M] [--json FILE]\n", argv[0]);
            return 2;
        }
    }

    if (nodes == 0 || nodes > MAX_NODES) {
        fprintf(stderr, "iot_stress: --nodes must be between 1 and %d\n", MAX_NODES);
        return 2;
    }
    if (nodes > SPARKPLUG_B_MAX_DEVICES) {
        fprintf(stderr,
                "iot_stress: %u nodes do not fit in the registry (max %d)\n",
                nodes, SPARKPLUG_B_MAX_DEVICES);
        return 2;
    }

    // The same order as the gateway: the state, the registry and the bridge.
    sparkplug_b_state_reset();
    if (sparkplug_b_devices_init() != ESP_OK) {
        fprintf(stderr, "iot_stress: the device registry did not initialise\n");
        return 1;
    }
    if (sparkplug_b_espnow_bridge_init("iiot-kit", "GW-STRESS") != ESP_OK) {
        fprintf(stderr, "iot_stress: the bridge did not initialise\n");
        return 1;
    }

    const long rss_before = rss_kib();

    unsigned int accepted = 0;
    unsigned int rejected = 0;
    unsigned int identity_errors = 0;
    unsigned int sequence_errors = 0;
    uint64_t last_seq = 0;
    bool sequence_started = false;

    char expected_id[SPARKPLUG_B_DEVICE_ID_MAX];

    for (unsigned int round = 0; round < rounds; ++round) {
        for (unsigned int node = 0; node < nodes; ++node) {
            uint8_t mac[6];
            node_mac(node, mac);

            msg_sensor_data_t frame;
            memset(&frame, 0, sizeof(frame));
            protocol_fill_header(&frame.header, MSG_TYPE_DATA, mac, round + 1,
                                 sizeof(frame) - sizeof(msg_header_t));
            frame.sensor_type = SENSOR_TYPE_TEMPERATURE;
            frame.value_count = 1;
            // A different value per node: a frame that is attributed to the
            // wrong node would be visible in the metric set of that node.
            frame.values[0] = 20.0f + (float)node;
            frame.uptime_ms = (uint64_t)round * 1000u;
            frame.battery_pct = 90;
            frame.rssi_last = -60;

            sparkplug_b_ddata_t out;
            memset(&out, 0, sizeof(out));

            if (sparkplug_b_espnow_bridge_on_data(&frame, &out) != ESP_OK) {
                ++rejected;
                continue;
            }
            ++accepted;

            // The MQTT layer registers the device when it publishes the
            // translated message (sparkplug_b_mqtt.c). The harness follows
            // the same path without the broker.
            sparkplug_b_devices_touch(out.device_id, mac, (uint64_t)round * 1000u + node);

            // Every frame of a node must carry the device id of that node.
            snprintf(expected_id, sizeof(expected_id), "N-%02X%02X", mac[4], mac[5]);
            if (strcmp(out.device_id, expected_id) != 0) {
                ++identity_errors;
                fprintf(stderr,
                        "iot_stress: node %u produced '%s' instead of '%s'\n",
                        node, out.device_id, expected_id);
            }

            // The sequence space advances once per published message. It is a
            // single byte, so it wraps from 255 back to 0
            // (Sparkplug B v3.0.0 §6.4.1).
            if (!sequence_started) {
                last_seq = out.seq;
                sequence_started = true;
            } else {
                const uint64_t expected = (last_seq + 1) % 256;
                if (out.seq != expected) {
                    ++sequence_errors;
                    fprintf(stderr,
                            "iot_stress: the sequence jumped from %llu to %llu (expected %llu)\n",
                            (unsigned long long)last_seq, (unsigned long long)out.seq,
                            (unsigned long long)expected);
                }
            }
            last_seq = out.seq;
        }
    }

    const size_t registered = sparkplug_b_devices_count();
    const long rss_after = rss_kib();
    const long rss_growth = rss_after > rss_before ? rss_after - rss_before : 0;

    // Every node must be in the registry, with its own MAC.
    unsigned int registry_errors = 0;
    for (unsigned int node = 0; node < nodes; ++node) {
        uint8_t mac[6];
        node_mac(node, mac);
        snprintf(expected_id, sizeof(expected_id), "N-%02X%02X", mac[4], mac[5]);

        size_t index = 0;
        if (sparkplug_b_devices_find(expected_id, &index) != ESP_OK) {
            ++registry_errors;
            fprintf(stderr, "iot_stress: %s is not in the registry\n", expected_id);
            continue;
        }

        sparkplug_b_device_t device;
        if (sparkplug_b_devices_get(index, &device) != ESP_OK) {
            ++registry_errors;
            continue;
        }
        if (memcmp(device.mac, mac, sizeof(mac)) != 0) {
            ++registry_errors;
            fprintf(stderr, "iot_stress: %s carries the MAC of another node\n", expected_id);
        }
    }

    const unsigned int total = accepted + rejected;
    const double loss_percent = total == 0 ? 100.0 : (double)rejected * 100.0 / (double)total;

    printf("{\n");
    printf("  \"nodes\": %u,\n", nodes);
    printf("  \"rounds\": %u,\n", rounds);
    printf("  \"frames_total\": %u,\n", total);
    printf("  \"frames_accepted\": %u,\n", accepted);
    printf("  \"frames_rejected\": %u,\n", rejected);
    printf("  \"loss_percent\": %.3f,\n", loss_percent);
    printf("  \"identity_errors\": %u,\n", identity_errors);
    printf("  \"sequence_errors\": %u,\n", sequence_errors);
    printf("  \"registry_devices\": %zu,\n", registered);
    printf("  \"registry_errors\": %u,\n", registry_errors);
    printf("  \"rss_before_kib\": %ld,\n", rss_before);
    printf("  \"rss_after_kib\": %ld,\n", rss_after);
    printf("  \"rss_growth_kib\": %ld\n", rss_growth);
    printf("}\n");

    if (json_path != NULL) {
        FILE *json = fopen(json_path, "w");
        if (json != NULL) {
            fprintf(json,
                    "{\"nodes\":%u,\"rounds\":%u,\"frames_total\":%u,\"frames_accepted\":%u,"
                    "\"frames_rejected\":%u,\"loss_percent\":%.3f,\"identity_errors\":%u,"
                    "\"sequence_errors\":%u,\"registry_devices\":%zu,\"registry_errors\":%u,"
                    "\"rss_before_kib\":%ld,\"rss_after_kib\":%ld,\"rss_growth_kib\":%ld}\n",
                    nodes, rounds, total, accepted, rejected, loss_percent, identity_errors,
                    sequence_errors, registered, registry_errors, rss_before, rss_after,
                    rss_growth);
            fclose(json);
        }
    }

    int failures = 0;
    if (rejected > 0) {
        fprintf(stderr, "iot_stress: %u frame(s) rejected (loss %.3f%%)\n", rejected, loss_percent);
        ++failures;
    }
    if (loss_percent > MAX_ACCEPTED_LOSS_PERCENT) {
        ++failures;
    }
    if (identity_errors > 0 || registry_errors > 0 || sequence_errors > 0) {
        ++failures;
    }
    if (registered != nodes) {
        fprintf(stderr, "iot_stress: the registry holds %zu devices, expected %u\n",
                registered, nodes);
        ++failures;
    }
    if (rss_growth > MAX_RSS_GROWTH_KIB) {
        fprintf(stderr, "iot_stress: the resident memory grew by %ld KiB, above the %d KiB limit\n",
                rss_growth, MAX_RSS_GROWTH_KIB);
        ++failures;
    }

    printf("iot_stress: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

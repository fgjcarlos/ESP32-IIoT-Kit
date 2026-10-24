#!/usr/bin/env python3
"""Sparkplug B v3.0.0 conformance harness.

The harness acts as a Sparkplug *Host Application* against an MQTT 3.1.1 broker
and asserts the message flow required by GitHub issue #11, acceptance criterion
CA-S6 ("harness de conformidad con broker de referencia y publisher Python de
prueba").

Two roles are used, mirroring the real Sparkplug topology:

* the **Host Application** subscribes to the whole Sparkplug namespace of the
  configured group, receives the messages and asserts them. It never publishes
  node traffic: it only publishes commands (NCMD/DCMD), because that is the
  Host's job;
* the **Edge Node** (only in ``simulate`` mode) publishes the birth, data and
  death messages of a node and of one of its devices, subscribes to the command
  topics, and registers the MQTT **Will** that carries the NDEATH.

Two modes are available, selected with ``--mode`` / ``MODE``:

``simulate`` (default)
    The simulated Edge Node publishes a spec-valid sequence (NBIRTH, DBIRTH,
    DDATA, NDATA, DDEATH) and the Host Application asserts it. The run ends by
    dropping the TCP connection of the Edge Node without sending an MQTT
    DISCONNECT, so the broker publishes the Will on its behalf and the Host
    asserts the resulting NDEATH. This mode needs nothing but a broker, so it
    is the one that can run in CI.

``observe``
    Nothing is published except an optional rebirth probe. The harness asserts
    what a real ESP32-S3 gateway publishes. Message *ordering* is tolerated
    (a DDATA may land before or after the DBIRTH), but every individual
    message is validated strictly.

Payloads are decoded with the official Eclipse Tahu schema
(``tahu/sparkplug_b.proto``, vendored verbatim), which is compiled to Python
bindings at image build time with ``grpc_tools.protoc``. Conformance is
therefore judged against the upstream schema rather than against a hand-rolled
approximation of it.

Exit codes:
    0  every assertion passed
    1  harness error (broker unreachable, malformed configuration, ...)
    2  at least one assertion failed
"""

from __future__ import annotations

import argparse
import logging
import os
import socket
import sys
import threading
import time
import urllib.parse
from dataclasses import dataclass
from typing import Callable, Iterable, NoReturn, Sequence

import paho.mqtt.client as mqtt
from google.protobuf.message import DecodeError

try:
    from tahu import sparkplug_b_pb2 as spb
except ImportError as exc:  # pragma: no cover - operator error path
    sys.stderr.write(
        "FATAL: the Sparkplug Protobuf bindings are missing.\n"
        "       Build the image (docker compose build client), or generate them\n"
        "       locally with:\n"
        "         python -m grpc_tools.protoc --proto_path=./tahu \\\n"
        "               --python_out=./tahu ./tahu/sparkplug_b.proto\n"
        f"       (import error: {exc})\n"
    )
    raise SystemExit(1) from exc


LOG = logging.getLogger("conformance")

# Sparkplug B namespace version prefix.
SPARKPLUG_VERSION = "spBv1.0"

# Tahu DataType enum values (see tahu/sparkplug_b.proto, enum DataType).
DATATYPE_INT8 = 1
DATATYPE_INT64 = 4
DATATYPE_FLOAT = 9
DATATYPE_DOUBLE = 10
DATATYPE_BOOLEAN = 11

# ---------------------------------------------------------------------------
# Names and aliases of the metrics, mirroring the firmware contract in
# firmware/gateway/components/sparkplug_b/include/sparkplug_b_payload.h and
# firmware/gateway/components/sparkplug_b/include/sparkplug_b_espnow_bridge.h
# so the harness asserts the very same names and aliases the gateway emits.
# ---------------------------------------------------------------------------

METRIC_BD_SEQ = "bdSeq"
METRIC_REBIRTH = "Node Control/Rebirth"
METRIC_NODE_REBOOT = "Node Control/Reboot"
METRIC_DEVICE_REBIRTH = "Device Control/Rebirth"
METRIC_DEVICE_REBOOT = "Device Control/Reboot"
METRIC_UPTIME = "Inputs/UptimeMs"
METRIC_DEV_UPTIME = "Diagnostics/UptimeMs"
METRIC_BATTERY = "Diagnostics/BatteryPercent"
METRIC_RSSI = "Diagnostics/RssiLast"
METRIC_SLEEP_INTERVAL = "Properties/SleepIntervalSec"
METRIC_TEMPERATURE = "Inputs/Temperature"

# Alias 0 is reserved by the spec, 1..6 are the typed sensor metrics, 7..10
# the Custom slots, 11 the uptime of the EoN-Node, 12..15 the metrics and
# properties of the Devices. The node and the device ranges are disjoint
# because every alias must be unique across the whole metric set of an
# Edge Node (tck-id-payloads-alias-uniqueness).
ALIAS_TEMPERATURE = 1
ALIAS_CUSTOM_0 = 7
ALIAS_UPTIME = 11
ALIAS_BATTERY = 12
ALIAS_RSSI = 13
ALIAS_DEV_UPTIME = 14
ALIAS_SLEEP_INTERVAL = 15

# Message types carried on a node topic (4 levels) and on a device topic
# (5 levels), per the Sparkplug B v3.0.0 topic namespace.
NODE_MESSAGE_TYPES = frozenset({"NBIRTH", "NDEATH", "NCMD", "NDATA", "STATE"})
DEVICE_MESSAGE_TYPES = frozenset({"DBIRTH", "DDEATH", "DCMD", "DDATA"})

EXIT_OK = 0
EXIT_HARNESS_ERROR = 1
EXIT_ASSERTION_FAILURE = 2


class HarnessError(RuntimeError):
    """Raised for harness-level problems (bad config, broker unreachable).

    These are distinct from assertion failures: a harness error means the run
    could not be carried out at all, an assertion failure means the observed
    behaviour did not conform.
    """


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------


class Report:
    """Collects PASS/FAIL assertions and prints them as they happen."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._checks = 0
        self._failures = 0

    def check(self, ok: bool, name: str, detail: str = "") -> bool:
        """Record one assertion and print it. Returns ``ok`` unchanged."""
        with self._lock:
            self._checks += 1
            if not ok:
                self._failures += 1
            status = "PASS" if ok else "FAIL"
            line = f"[{status}] {name}"
            if detail:
                line += f" -- {detail}"
            print(line, flush=True)
        return ok

    def note(self, message: str) -> None:
        """Print an informational line that is not an assertion."""
        print(f"[INFO] {message}", flush=True)

    @property
    def checks(self) -> int:
        return self._checks

    @property
    def failures(self) -> int:
        return self._failures

    def summary(self) -> str:
        passed = self._checks - self._failures
        verdict = "PASS" if self._failures == 0 else "FAIL"
        return (
            f"Conformance summary: {self._checks} checks, {passed} passed, "
            f"{self._failures} failed -> {verdict}"
        )


# ---------------------------------------------------------------------------
# Topics
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class SparkplugTopic:
    """A parsed Sparkplug topic."""

    raw: str
    group_id: str
    message_type: str
    edge_node_id: str | None
    device_id: str | None


def parse_topic(raw: str) -> SparkplugTopic:
    """Parse a Sparkplug topic.

    Raises ``ValueError`` when the topic does not follow the Sparkplug B
    namespace shape, so callers can assert on topic correctness.
    """
    parts = raw.split("/")
    if len(parts) < 4:
        raise ValueError(f"too few topic levels: {raw!r}")
    if parts[0] != SPARKPLUG_VERSION:
        raise ValueError(f"unexpected namespace prefix: {raw!r}")
    group_id, message_type = parts[1], parts[2]
    if not group_id or not message_type:
        raise ValueError(f"empty group id or message type: {raw!r}")
    if message_type in NODE_MESSAGE_TYPES:
        if len(parts) != 4:
            raise ValueError(f"node message must have 4 levels: {raw!r}")
        return SparkplugTopic(raw, group_id, message_type, parts[3], None)
    if message_type in DEVICE_MESSAGE_TYPES:
        if len(parts) != 5:
            raise ValueError(f"device message must have 5 levels: {raw!r}")
        return SparkplugTopic(raw, group_id, message_type, parts[3], parts[4])
    raise ValueError(f"unknown Sparkplug message type {message_type!r}: {raw!r}")


# ---------------------------------------------------------------------------
# Message collection
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Received:
    """One message delivered by the broker.

    ``qos`` and ``retain`` are the values the broker delivered, which is how
    the harness observes the QoS and the retain flag a publisher used: MQTT
    delivers at ``min(publish QoS, subscription QoS)``, and the Host
    Application subscribes at QoS 1.
    """

    topic: str
    payload: bytes
    received_at: float
    qos: int
    retain: bool


class Collector:
    """Thread-safe store of broker-delivered messages.

    Matching scans *everything* received so far, not just the newest message.
    That is what makes the harness tolerant of message ordering: a DDATA that
    lands before the DBIRTH is still matched by the DBIRTH wait.
    """

    def __init__(self) -> None:
        self._condition = threading.Condition()
        self._items: list[Received] = []

    def add(self, item: Received) -> None:
        with self._condition:
            self._items.append(item)
            self._condition.notify_all()

    def snapshot(self) -> list[Received]:
        with self._condition:
            return list(self._items)

    def wait_until(
        self,
        predicate: Callable[[list[Received]], bool],
        deadline: float,
    ) -> bool:
        """Block until ``predicate`` holds for the received set, or the deadline."""
        with self._condition:
            while True:
                if predicate(list(self._items)):
                    return True
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return False
                self._condition.wait(min(remaining, 0.25))


class SeqTracker:
    """Tracks the sequence number of an Edge Node.

    Sparkplug B v3.0.0 defines **one** sequence space per Edge Node: the
    EoN-Node and all of its Devices share it, and every message published
    after an NBIRTH carries the previous sequence number plus one, wrapping
    from 255 to 0 (tck-id-payloads-sequence-num-incrementing). An NBIRTH opens
    a new sequence space with its own sequence number, which must be between
    0 and 255 (tck-id-payloads-sequence-num-zero-nbirth).

    Duplicate deliveries are tolerated: QoS 1 may redeliver a message, and a
    redelivery carries the very same sequence number and payload.
    """

    def __init__(self) -> None:
        self._expected: dict[str, int] = {}
        self._last: dict[str, tuple[int, bytes]] = {}

    def check(
        self,
        report: Report,
        label: str,
        edge_node_id: str,
        seq: int,
        payload: bytes,
        is_birth: bool = False,
    ) -> None:
        if is_birth:
            report.check(
                0 <= seq <= 255,
                f"{label} carries a sequence number between 0 and 255",
                f"seq={seq}",
            )
            self._expected[edge_node_id] = (seq + 1) % 256
            self._last[edge_node_id] = (seq, payload)
            return

        previous = self._last.get(edge_node_id)
        if previous is not None and previous == (seq, payload):
            report.check(True, f"{label} carries sequence number {seq}", "duplicate delivery, ignored")
            return

        expected = self._expected.get(edge_node_id)
        if expected is None:
            report.check(
                False,
                f"{label} continues the sequence space of the edge node",
                "no NBIRTH was observed, so there is no sequence space to continue",
            )
            self._last[edge_node_id] = (seq, payload)
            return

        report.check(
            seq == expected,
            f"{label} continues the sequence space of the edge node",
            f"expected={expected} current={seq}",
        )
        self._expected[edge_node_id] = (seq + 1) % 256
        self._last[edge_node_id] = (seq, payload)


class AliasRegistry:
    """Tracks the aliases bound by the BIRTH messages of an Edge Node.

    Every alias supplied in an NBIRTH or a DBIRTH must be unique across the
    whole set of metrics of that Edge Node
    (tck-id-payloads-alias-uniqueness), so the harness keeps one registry per
    Edge Node and checks every BIRTH against it.
    """

    def __init__(self) -> None:
        self._aliases: dict[str, dict[int, str]] = {}

    def bind(self, edge_node_id: str, alias: int, name: str) -> None:
        self._aliases.setdefault(edge_node_id, {})[alias] = name

    def aliases_of(self, edge_node_id: str) -> dict[int, str]:
        return self._aliases.get(edge_node_id, {})

    def known(self, edge_node_id: str, alias: int) -> bool:
        return alias in self._aliases.get(edge_node_id, {})


# ---------------------------------------------------------------------------
# Payload helpers
# ---------------------------------------------------------------------------


def build_metric(
    *,
    name: str | None = None,
    alias: int | None = None,
    datatype: int | None = None,
    long_value: int | None = None,
    double_value: float | None = None,
    float_value: float | None = None,
    int_value: int | None = None,
    boolean_value: bool | None = None,
) -> spb.Payload.Metric:
    """Build one Tahu Metric.

    The schema is proto2 with a ``oneof value``, so exactly one value field
    may be set. Omitting every value yields a metric that only carries
    metadata, which is what the alias-only DATA metrics need.
    """
    metric = spb.Payload.Metric()
    if name is not None:
        metric.name = name
    if alias is not None:
        metric.alias = alias
    if datatype is not None:
        metric.datatype = datatype
    if long_value is not None:
        metric.long_value = long_value
    elif double_value is not None:
        metric.double_value = double_value
    elif float_value is not None:
        metric.float_value = float_value
    elif int_value is not None:
        # The Tahu schema declares int_value as a uint32, so a negative Int8
        # travels as its two's complement bit pattern widened to 32 bits. The
        # firmware encodes it exactly the same way.
        metric.int_value = int_value & 0xFFFFFFFF
    elif boolean_value is not None:
        metric.boolean_value = boolean_value
    return metric


def build_payload(
    seq: int | None,
    metrics: Sequence[spb.Payload.Metric],
    timestamp_ms: int | None = None,
) -> bytes:
    """Serialise a Tahu Payload.

    ``seq`` may be ``None``: the NDEATH is the only Sparkplug message that
    must not carry a sequence number (tck-id-topics-ndeath-seq).
    """
    payload = spb.Payload()
    payload.timestamp = timestamp_ms if timestamp_ms is not None else int(time.time() * 1000)
    if seq is not None:
        payload.seq = seq
    for metric in metrics:
        payload.metrics.add().CopyFrom(metric)
    return payload.SerializeToString()


def decode_payload(raw: bytes) -> spb.Payload:
    """Decode a Tahu Payload. Raises ``DecodeError`` on malformed input."""
    payload = spb.Payload()
    payload.ParseFromString(raw)
    return payload


def metric_value(metric: spb.Payload.Metric) -> object | None:
    """Return the value carried by a metric, or ``None`` when it has none."""
    kind = metric.WhichOneof("value")
    if kind is None:
        return None
    return getattr(metric, kind)


def find_metric(payload: spb.Payload, name: str) -> spb.Payload.Metric | None:
    """Return the metric with the given name, or ``None`` when it is absent."""
    for metric in payload.metrics:
        if metric.HasField("name") and metric.name == name:
            return metric
    return None


def find_bd_seq(payload: spb.Payload) -> int | None:
    """Return the bdSeq metric value, or ``None`` when the metric is absent.

    ``bdSeq`` is the birth-death sequence number. Per the Sparkplug B v3.0.0
    specification it travels as a metric *named* ``bdSeq`` with an INT64
    datatype, and the reference example of the specification leaves its alias
    empty.
    """
    metric = find_metric(payload, METRIC_BD_SEQ)
    if metric is None:
        return None
    kind = metric.WhichOneof("value")
    if kind == "long_value":
        return int(metric.long_value)
    if kind == "int_value":
        return int(metric.int_value)
    return None


def describe(payload: spb.Payload) -> str:
    """Short human-readable summary of a decoded payload."""
    metrics = []
    for metric in payload.metrics:
        name = metric.name if metric.HasField("name") else f"alias:{metric.alias}"
        value = metric_value(metric)
        metrics.append(f"{name}={value}")
    seq = payload.seq if payload.HasField("seq") else "-"
    return f"seq={seq} metrics=[{', '.join(metrics)}]"


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Config:
    """Resolved harness configuration."""

    mode: str
    broker_uri: str
    group_id: str
    edge_node_id: str
    device_id: str
    timeout: float
    expect_device: bool
    expect_ddeath: bool
    expect_ndata: bool
    expect_rebirth: bool


def parse_broker_uri(uri: str) -> tuple[str, int]:
    """Split an ``mqtt://host:port`` URI into host and port."""
    candidate = uri if "://" in uri else f"mqtt://{uri}"
    parsed = urllib.parse.urlparse(candidate)
    host = parsed.hostname or "localhost"
    try:
        port = parsed.port or 1883
    except ValueError as exc:
        raise HarnessError(f"invalid broker port in {uri!r}") from exc
    return host, port


# ---------------------------------------------------------------------------
# MQTT clients
# ---------------------------------------------------------------------------


class MqttClientBase:
    """Shared connection handling for the two MQTT clients of the harness."""

    def __init__(self, config: Config, report: Report, client_id: str) -> None:
        self.config = config
        self.report = report
        self.collector = Collector()
        self._connected = threading.Event()
        self._subscribed = threading.Event()
        self._connect_error: str | None = None
        self._subscribed_topics: list[str] = []

        self.client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2,
            client_id=client_id,
            protocol=mqtt.MQTTv311,
        )
        self.client.on_connect = self._on_connect
        self.client.on_subscribe = self._on_subscribe
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect

    # -- callbacks ----------------------------------------------------------
    def _on_connect(self, client, userdata, flags, reason_code, properties) -> None:
        # paho 2.x hands the CONNACK reason code as a ReasonCode object, which
        # compares equal to its numeric value.
        if reason_code == 0:
            LOG.info("connected to broker (CONNACK success)")
        else:
            self._connect_error = str(reason_code)
            LOG.error("broker refused the connection: %s", reason_code)
        self._connected.set()

    def _on_subscribe(self, client, userdata, mid, reason_codes, properties) -> None:
        LOG.info("subscription acknowledged (mid=%s)", mid)
        self._subscribed.set()

    def _on_message(self, client, userdata, message) -> None:
        self.collector.add(
            Received(
                message.topic,
                bytes(message.payload),
                time.monotonic(),
                int(message.qos),
                bool(message.retain),
            )
        )

    def _on_disconnect(self, client, userdata, flags, reason_code, properties) -> None:
        LOG.warning("disconnected from broker: %s", reason_code)

    # -- connection ---------------------------------------------------------
    def connect(self, keepalive: int = 60) -> None:
        """Connect to the broker, retrying until the timeout expires.

        A retry loop is used instead of a fixed sleep so the harness does not
        depend on the broker being up the instant the container starts.
        """
        host, port = parse_broker_uri(self.config.broker_uri)
        deadline = time.monotonic() + self.config.timeout
        attempt = 0
        last_error: Exception | None = None
        while True:
            attempt += 1
            try:
                self.client.connect(host, port, keepalive=keepalive)
                self.client.loop_start()
                break
            except (OSError, socket.error) as exc:
                last_error = exc
                if time.monotonic() >= deadline:
                    raise HarnessError(
                        f"cannot reach the MQTT broker at {host}:{port} within "
                        f"{self.config.timeout:.0f}s: {last_error}"
                    ) from exc
                LOG.info("broker not ready yet (attempt %d): %s", attempt, exc)
                time.sleep(0.5)

        if not self._connected.wait(timeout=self.config.timeout):
            raise HarnessError(
                f"broker at {host}:{port} accepted the socket but sent no CONNACK "
                f"within {self.config.timeout:.0f}s"
            )
        if self._connect_error:
            raise HarnessError(f"broker refused the connection: {self._connect_error}")
        LOG.info("connected to %s:%d", host, port)

    def subscribe(self, topic: str, qos: int) -> str:
        """Subscribe to a topic and wait for the SUBACK."""
        result, _mid = self.client.subscribe(topic, qos=qos)
        if result != mqtt.MQTT_ERR_SUCCESS:
            raise HarnessError(f"cannot subscribe to {topic}: MQTT error {result}")
        if not self._subscribed.wait(timeout=self.config.timeout):
            raise HarnessError(f"broker did not acknowledge the subscription to {topic}")
        LOG.info("subscribed to %s (qos %d)", topic, qos)
        self._subscribed_topics.append(topic)
        return topic

    def publish(self, topic: str, payload: bytes, qos: int, retain: bool = False) -> bool:
        """Publish a message and wait for the broker's acknowledgement."""
        info = self.client.publish(topic, payload, qos=qos, retain=retain)
        if info.rc != mqtt.MQTT_ERR_SUCCESS:
            return self.report.check(
                False,
                f"publish to {topic} is accepted by the client library",
                f"MQTT error {info.rc}",
            )
        try:
            info.wait_for_publish(timeout=self.config.timeout)
        except RuntimeError as exc:
            return self.report.check(False, f"broker acknowledged the publish on {topic}", str(exc))
        return self.report.check(
            info.is_published(),
            f"broker acknowledged the publish on {topic}",
            "" if info.is_published() else "no acknowledgement before the timeout",
        )

    def disconnect(self) -> None:
        try:
            self.client.disconnect()
            self.client.loop_stop()
        except Exception as exc:  # pragma: no cover - best-effort teardown
            LOG.debug("error during disconnect: %s", exc)


class SparkplugHost(MqttClientBase):
    """MQTT client playing the Sparkplug Host Application role.

    The Host Application subscribes to the whole namespace of its group at
    QoS 1, so a message delivered at QoS 0 proves the publisher used QoS 0 and
    a message delivered at QoS 1 proves the publisher used QoS 1.
    """

    def __init__(self, config: Config, report: Report) -> None:
        super().__init__(config, report, f"sp-host-{os.getpid()}")
        self.malformed_topics: list[str] = []
        self._malformed_lock = threading.Lock()

    def _on_message(self, client, userdata, message) -> None:
        try:
            parse_topic(message.topic)
        except ValueError:
            with self._malformed_lock:
                self.malformed_topics.append(message.topic)
        super()._on_message(client, userdata, message)

    def subscribe_namespace(self) -> str:
        """Subscribe to the whole Sparkplug namespace of the configured group."""
        topic = f"{SPARKPLUG_VERSION}/{self.config.group_id}/#"
        self.subscribe(topic, qos=1)
        return topic


class SparkplugNode(MqttClientBase):
    """MQTT client playing the Sparkplug Edge Node role.

    Only used in ``simulate`` mode. It publishes the birth, data and death
    messages exactly as the firmware does and registers the Will that carries
    the NDEATH.
    """

    def __init__(self, config: Config, report: Report) -> None:
        super().__init__(config, report, f"sp-node-{os.getpid()}")

    def register_will(self, topic: str, payload: bytes) -> None:
        """Register the MQTT Will of the Edge Node.

        The spec requires the Will to carry the NDEATH at QoS 1 with the
        retain flag set to false
        (tck-id-payloads-ndeath-will-message-qos, -will-message-retain).
        """
        self.client.will_set(topic, payload, qos=1, retain=False)
        LOG.info("registered the Will on %s (qos 1, retain false)", topic)

    def kill_connection(self) -> None:
        """Drop the TCP connection without sending an MQTT DISCONNECT.

        A clean DISCONNECT tells the broker not to publish the Will, and the
        whole point of the exercise is to let the broker publish it, exactly as
        it would after a crash or a power loss.
        """
        try:
            # The network loop is stopped first so nothing reconnects after the
            # socket is gone. The join can take up to one keepalive period.
            self.client.loop_stop()
        except Exception as exc:  # pragma: no cover - best-effort teardown
            LOG.debug("error while stopping the network loop: %s", exc)
        sock = self.client.socket()
        if sock is None:
            return
        try:
            sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            sock.close()
        except OSError:  # pragma: no cover - best-effort teardown
            pass
        LOG.info("dropped the TCP connection without a DISCONNECT")


# ---------------------------------------------------------------------------
# Validation helpers
# ---------------------------------------------------------------------------


def validate_birth(
    report: Report,
    label: str,
    payload: spb.Payload,
    *,
    edge_node_id: str,
    require_bd_seq: bool,
    require_rebirth_metric: bool,
    registry: AliasRegistry,
) -> None:
    """Assert the shape a Sparkplug BIRTH payload must have.

    ``tck-id-payloads-alias-birth-requirement`` says a BIRTH message carries
    both the name and the alias of every metric. The two metrics that Sparkplug
    itself identifies by name are the documented exceptions: ``bdSeq`` (the
    reference example of the specification leaves its alias empty) and
    ``Node Control/Rebirth``, whose alias MUST be absent
    (tck-id-operational-behavior-data-commands-rebirth-name-aliases).

    ``require_bd_seq`` is true for the NBIRTH only: the specification demands
    the ``bdSeq`` metric there, while a DBIRTH carries the metrics of the
    Device and does not have to repeat the birth generation.
    """
    report.check(payload.HasField("seq"), f"{label} payload carries a sequence number")
    report.check(len(payload.metrics) > 0, f"{label} payload carries at least one metric")
    report.check(
        all(metric.HasField("name") for metric in payload.metrics),
        f"{label} metrics define their names",
        "names are mandatory on BIRTH messages",
    )
    report.check(
        all(metric.HasField("datatype") for metric in payload.metrics),
        f"{label} metrics declare their datatype",
    )

    # -- bdSeq --------------------------------------------------------------
    bd_seq_metric = find_metric(payload, METRIC_BD_SEQ)
    if require_bd_seq:
        report.check(
            bd_seq_metric is not None,
            f"{label} payload carries the {METRIC_BD_SEQ} metric",
            "bdSeq marks the birth generation of the session",
        )
    if bd_seq_metric is not None:
        report.check(
            bd_seq_metric.datatype == DATATYPE_INT64,
            f"{label} {METRIC_BD_SEQ} metric has the INT64 datatype",
            f"datatype={bd_seq_metric.datatype}",
        )
        report.check(
            metric_value(bd_seq_metric) is not None,
            f"{label} {METRIC_BD_SEQ} metric carries a value",
        )
        report.check(
            not bd_seq_metric.HasField("alias"),
            f"{label} {METRIC_BD_SEQ} metric carries no alias",
            "the reference example of the specification leaves it empty",
        )

    # -- Node Control/Rebirth ----------------------------------------------
    rebirth_metric = find_metric(payload, METRIC_REBIRTH)
    if require_rebirth_metric:
        report.check(
            rebirth_metric is not None,
            f"{label} payload carries the {METRIC_REBIRTH} metric",
            "a Host Application uses it to request a rebirth",
        )
        if rebirth_metric is not None:
            report.check(
                rebirth_metric.datatype == DATATYPE_BOOLEAN,
                f"{label} {METRIC_REBIRTH} metric has the Boolean datatype",
                f"datatype={rebirth_metric.datatype}",
            )
            report.check(
                metric_value(rebirth_metric) is False,
                f"{label} {METRIC_REBIRTH} metric is false",
                f"value={metric_value(rebirth_metric)}",
            )
            report.check(
                not rebirth_metric.HasField("alias"),
                f"{label} {METRIC_REBIRTH} metric carries no alias",
                "the metric is identified by its name",
            )

    # -- names and aliases --------------------------------------------------
    control_metrics = {METRIC_BD_SEQ, METRIC_REBIRTH}
    others = [metric for metric in payload.metrics if metric.name not in control_metrics]
    report.check(
        all(metric.HasField("alias") for metric in others),
        f"{label} metrics define their aliases",
        "every metric but the named control ones carries an alias",
    )

    # -- alias uniqueness across the Edge Node ------------------------------
    known = registry.aliases_of(edge_node_id)
    collisions = [
        (metric.alias, metric.name, known[metric.alias])
        for metric in others
        if metric.HasField("alias") and metric.alias in known
    ]
    report.check(
        not collisions,
        f"{label} metrics reuse no alias of the edge node",
        "; ".join(f"alias {a}: '{n}' vs '{o}'" for a, n, o in collisions),
    )
    for metric in others:
        if metric.HasField("alias"):
            registry.bind(edge_node_id, metric.alias, metric.name)


def validate_data(report: Report, label: str, payload: spb.Payload) -> None:
    """Assert the shape a Sparkplug DATA payload must have.

    ``tck-id-payloads-alias-data-cmd-requirement`` says a DATA message MUST
    only include an alias and the metric name MUST be excluded: the friendly
    name was already bound by the BIRTH message.
    """
    report.check(payload.HasField("seq"), f"{label} payload carries a sequence number")
    report.check(len(payload.metrics) > 0, f"{label} payload carries at least one metric")
    report.check(
        all(metric.HasField("alias") for metric in payload.metrics),
        f"{label} metrics reference an alias",
        "aliases are mandatory on DATA messages",
    )
    named = [metric.name for metric in payload.metrics if metric.HasField("name")]
    report.check(
        not named,
        f"{label} metrics carry no name",
        f"names={named}" if named else "the name was already established by the BIRTH",
    )


def validate_ndeath(
    report: Report,
    label: str,
    payload: spb.Payload,
    *,
    expected_bd_seq: int | None,
) -> None:
    """Assert the shape a Sparkplug NDEATH payload must have.

    The NDEATH carries only the ``bdSeq`` metric and MUST NOT include a
    sequence number (tck-id-topics-ndeath-payload, tck-id-topics-ndeath-seq),
    and its ``bdSeq`` must match the one of the NBIRTH of the same session
    (tck-id-payloads-ndeath-bdseq).
    """
    report.check(
        not payload.HasField("seq"),
        f"{label} payload carries no sequence number",
        "the NDEATH is the only Sparkplug message without one",
    )
    report.check(
        len(payload.metrics) == 1,
        f"{label} payload carries exactly one metric",
        f"metrics={len(payload.metrics)}",
    )
    bd_seq_metric = find_metric(payload, METRIC_BD_SEQ)
    report.check(
        bd_seq_metric is not None,
        f"{label} payload carries the {METRIC_BD_SEQ} metric",
    )
    if bd_seq_metric is not None:
        report.check(
            not bd_seq_metric.HasField("alias"),
            f"{label} {METRIC_BD_SEQ} metric carries no alias",
        )
        report.check(
            bd_seq_metric.datatype == DATATYPE_INT64,
            f"{label} {METRIC_BD_SEQ} metric has the INT64 datatype",
            f"datatype={bd_seq_metric.datatype}",
        )
        value = find_bd_seq(payload)
        if expected_bd_seq is not None:
            report.check(
                value == expected_bd_seq,
                f"{label} {METRIC_BD_SEQ} matches the {METRIC_BD_SEQ} of the NBIRTH",
                f"nbirth={expected_bd_seq} ndeath={value}",
            )


def validate_ddeath(report: Report, label: str, payload: spb.Payload) -> None:
    """Assert the shape a Sparkplug DDEATH payload must have.

    A DDEATH MUST include a sequence number
    (tck-id-payloads-ddeath-seq-number) and carries the ``bdSeq`` of the
    session the device belonged to.
    """
    report.check(payload.HasField("seq"), f"{label} payload carries a sequence number")
    report.check(len(payload.metrics) > 0, f"{label} payload carries at least one metric")
    bd_seq_metric = find_metric(payload, METRIC_BD_SEQ)
    report.check(
        bd_seq_metric is not None,
        f"{label} payload carries the {METRIC_BD_SEQ} metric",
    )
    if bd_seq_metric is not None:
        report.check(
            not bd_seq_metric.HasField("alias"),
            f"{label} {METRIC_BD_SEQ} metric carries no alias",
        )


def validate_delivery(report: Report, label: str, item: Received, *, qos: int) -> None:
    """Assert the QoS and the retain flag a message was delivered with."""
    report.check(
        item.qos == qos,
        f"{label} was published with QoS {qos}",
        f"delivered with QoS {item.qos}",
    )
    report.check(
        not item.retain,
        f"{label} was published with the retain flag set to false",
        "delivered with the retain flag set" if item.retain else "",
    )


def find_received(
    items: Iterable[Received],
    *,
    message_type: str,
    edge_node_id: str,
    device_id: str | None = None,
) -> Received | None:
    """Return the first received message of a kind, or ``None``.

    ``device_id`` narrows the search to a single Device. When it is ``None``
    any Device of the Edge Node matches, which is what the observe mode needs:
    the device id of a real gateway is not known in advance.
    """
    for item in items:
        try:
            topic = parse_topic(item.topic)
        except ValueError:
            continue
        if topic.message_type != message_type or topic.edge_node_id != edge_node_id:
            continue
        if message_type in DEVICE_MESSAGE_TYPES and device_id is not None:
            if topic.device_id != device_id:
                continue
        return item
    return None


def has_type(items: Iterable[Received], message_type: str, edge_node_id: str) -> bool:
    """Tell whether a message of a kind was received."""
    return find_received(items, message_type=message_type, edge_node_id=edge_node_id) is not None


# ---------------------------------------------------------------------------
# simulate mode
# ---------------------------------------------------------------------------


def run_simulate(host: SparkplugHost, node: SparkplugNode, report: Report) -> None:
    """Publish a spec-valid sequence and assert it from the Host Application."""
    config = host.config
    group, edge, device = config.group_id, config.edge_node_id, config.device_id
    tracker = SeqTracker()
    registry = AliasRegistry()

    report.note(
        f"simulating edge node '{edge}' with device '{device}' in group '{group}'"
    )

    node_topic = lambda verb: f"{SPARKPLUG_VERSION}/{group}/{verb}/{edge}"  # noqa: E731
    device_topic = lambda verb: f"{SPARKPLUG_VERSION}/{group}/{verb}/{edge}/{device}"  # noqa: E731

    bd_seq = 7

    # -- the Edge Node connects and registers its Will ----------------------
    node_ndeath_topic = node_topic("NDEATH")
    will_payload = build_payload(
        seq=None,
        metrics=[build_metric(name=METRIC_BD_SEQ, datatype=DATATYPE_INT64, long_value=bd_seq)],
    )
    node.register_will(node_ndeath_topic, will_payload)
    node.connect(keepalive=3)
    node.subscribe(node_topic("NCMD"), qos=1)
    node.subscribe(device_topic("DCMD"), qos=1)

    # -- NBIRTH -------------------------------------------------------------
    nbirth_topic = node_topic("NBIRTH")
    nbirth_metrics = [
        build_metric(name=METRIC_BD_SEQ, datatype=DATATYPE_INT64, long_value=bd_seq),
        build_metric(name=METRIC_REBIRTH, datatype=DATATYPE_BOOLEAN, boolean_value=False),
        build_metric(
            name=METRIC_UPTIME,
            alias=ALIAS_UPTIME,
            datatype=DATATYPE_INT64,
            long_value=1234,
        ),
    ]
    report.note(f"publishing {nbirth_topic} (seq 0)")
    node.publish(nbirth_topic, build_payload(seq=0, metrics=nbirth_metrics), qos=0)

    received = host.collector.wait_until(
        lambda items: has_type(items, "NBIRTH", edge),
        time.monotonic() + config.timeout,
    )
    if not report.check(received, f"NBIRTH reaches the host application on {nbirth_topic}"):
        return
    nbirth = find_received(host.collector.snapshot(), message_type="NBIRTH", edge_node_id=edge)
    assert nbirth is not None
    try:
        payload = decode_payload(nbirth.payload)
    except DecodeError as exc:
        report.check(False, "NBIRTH payload decodes with the Tahu schema", str(exc))
        return
    report.check(True, "NBIRTH payload decodes with the Tahu schema", describe(payload))
    validate_birth(
        report,
        "NBIRTH",
        payload,
        edge_node_id=edge,
        require_bd_seq=True,
        require_rebirth_metric=True,
        registry=registry,
    )
    validate_delivery(report, "NBIRTH", nbirth, qos=0)
    tracker.check(report, "NBIRTH", edge, payload.seq, nbirth.payload, is_birth=True)

    # -- DBIRTH -------------------------------------------------------------
    dbirth_topic = device_topic("DBIRTH")
    dbirth_metrics = [
        build_metric(
            name=METRIC_TEMPERATURE,
            alias=ALIAS_TEMPERATURE,
            datatype=DATATYPE_DOUBLE,
            double_value=23.5,
        ),
        build_metric(
            name="Inputs/Custom/0",
            alias=ALIAS_CUSTOM_0,
            datatype=DATATYPE_DOUBLE,
            double_value=24.5,
        ),
        build_metric(
            name=METRIC_DEV_UPTIME,
            alias=ALIAS_DEV_UPTIME,
            datatype=DATATYPE_INT64,
            long_value=4321,
        ),
        build_metric(
            name=METRIC_BATTERY,
            alias=ALIAS_BATTERY,
            datatype=DATATYPE_FLOAT,
            float_value=87.0,
        ),
        build_metric(
            name=METRIC_RSSI,
            alias=ALIAS_RSSI,
            datatype=DATATYPE_INT8,
            int_value=-65,
        ),
        build_metric(
            name=METRIC_SLEEP_INTERVAL,
            alias=ALIAS_SLEEP_INTERVAL,
            datatype=DATATYPE_INT64,
            long_value=300,
        ),
    ]
    report.note(f"publishing {dbirth_topic} (seq 1)")
    node.publish(dbirth_topic, build_payload(seq=1, metrics=dbirth_metrics), qos=0)

    received = host.collector.wait_until(
        lambda items: has_type(items, "DBIRTH", edge),
        time.monotonic() + config.timeout,
    )
    if not report.check(received, f"DBIRTH reaches the host application on {dbirth_topic}"):
        return
    dbirth = find_received(host.collector.snapshot(), message_type="DBIRTH", edge_node_id=edge)
    assert dbirth is not None
    try:
        payload = decode_payload(dbirth.payload)
    except DecodeError as exc:
        report.check(False, "DBIRTH payload decodes with the Tahu schema", str(exc))
        return
    report.check(True, "DBIRTH payload decodes with the Tahu schema", describe(payload))
    validate_birth(
        report,
        "DBIRTH",
        payload,
        edge_node_id=edge,
        require_bd_seq=False,
        require_rebirth_metric=False,
        registry=registry,
    )
    validate_delivery(report, "DBIRTH", dbirth, qos=0)
    tracker.check(report, "DBIRTH", edge, payload.seq, dbirth.payload)

    # -- DDATA --------------------------------------------------------------
    ddata_topic = device_topic("DDATA")
    ddata_metrics = [
        build_metric(alias=ALIAS_TEMPERATURE, datatype=DATATYPE_DOUBLE, double_value=24.75),
        build_metric(alias=ALIAS_DEV_UPTIME, datatype=DATATYPE_INT64, long_value=5678),
    ]
    report.note(f"publishing {ddata_topic} (seq 2)")
    node.publish(ddata_topic, build_payload(seq=2, metrics=ddata_metrics), qos=0)

    received = host.collector.wait_until(
        lambda items: has_type(items, "DDATA", edge),
        time.monotonic() + config.timeout,
    )
    if not report.check(received, f"DDATA reaches the host application on {ddata_topic}"):
        return
    ddata = find_received(host.collector.snapshot(), message_type="DDATA", edge_node_id=edge)
    assert ddata is not None
    try:
        payload = decode_payload(ddata.payload)
    except DecodeError as exc:
        report.check(False, "DDATA payload decodes with the Tahu schema", str(exc))
        return
    report.check(True, "DDATA payload decodes with the Tahu schema", describe(payload))
    validate_data(report, "DDATA", payload)
    validate_delivery(report, "DDATA", ddata, qos=0)
    tracker.check(report, "DDATA", edge, payload.seq, ddata.payload)

    # -- NDATA --------------------------------------------------------------
    ndata_topic = node_topic("NDATA")
    ndata_metrics = [
        build_metric(alias=ALIAS_UPTIME, datatype=DATATYPE_INT64, long_value=7890),
    ]
    report.note(f"publishing {ndata_topic} (seq 3)")
    node.publish(ndata_topic, build_payload(seq=3, metrics=ndata_metrics), qos=0)

    received = host.collector.wait_until(
        lambda items: has_type(items, "NDATA", edge),
        time.monotonic() + config.timeout,
    )
    if not report.check(received, f"NDATA reaches the host application on {ndata_topic}"):
        return
    ndata = find_received(host.collector.snapshot(), message_type="NDATA", edge_node_id=edge)
    assert ndata is not None
    try:
        payload = decode_payload(ndata.payload)
    except DecodeError as exc:
        report.check(False, "NDATA payload decodes with the Tahu schema", str(exc))
        return
    report.check(True, "NDATA payload decodes with the Tahu schema", describe(payload))
    validate_data(report, "NDATA", payload)
    validate_delivery(report, "NDATA", ndata, qos=0)
    tracker.check(report, "NDATA", edge, payload.seq, ndata.payload)

    # -- DDEATH -------------------------------------------------------------
    ddeath_topic = device_topic("DDEATH")
    ddeath_metrics = [
        build_metric(name=METRIC_BD_SEQ, datatype=DATATYPE_INT64, long_value=bd_seq),
    ]
    report.note(f"publishing {ddeath_topic} (seq 4)")
    node.publish(ddeath_topic, build_payload(seq=4, metrics=ddeath_metrics), qos=0)

    received = host.collector.wait_until(
        lambda items: has_type(items, "DDEATH", edge),
        time.monotonic() + config.timeout,
    )
    if report.check(received, f"DDEATH reaches the host application on {ddeath_topic}"):
        ddeath = find_received(host.collector.snapshot(), message_type="DDEATH", edge_node_id=edge)
        assert ddeath is not None
        try:
            payload = decode_payload(ddeath.payload)
        except DecodeError as exc:
            report.check(False, "DDEATH payload decodes with the Tahu schema", str(exc))
        else:
            report.check(True, "DDEATH payload decodes with the Tahu schema", describe(payload))
            validate_ddeath(report, "DDEATH", payload)
            validate_delivery(report, "DDEATH", ddeath, qos=0)
            tracker.check(report, "DDEATH", edge, payload.seq, ddeath.payload)

    # -- NCMD ---------------------------------------------------------------
    # A Rebirth Request MUST use the NCMD verb and MUST include a metric
    # named "Node Control/Rebirth" with the value true.
    ncmd_topic = node_topic("NCMD")
    ncmd_metrics = [
        build_metric(name=METRIC_REBIRTH, datatype=DATATYPE_BOOLEAN, boolean_value=True),
    ]
    report.note(f"publishing {ncmd_topic}")
    host.publish(ncmd_topic, build_payload(seq=0, metrics=ncmd_metrics), qos=1)
    received = node.collector.wait_until(
        lambda items: any(item.topic == ncmd_topic for item in items),
        time.monotonic() + config.timeout,
    )
    if report.check(received, f"NCMD reaches the edge node on {ncmd_topic}"):
        ncmd = next(item for item in node.collector.snapshot() if item.topic == ncmd_topic)
        report.check(
            ncmd.qos == 1,
            "the edge node receives the NCMD at QoS 1",
            f"delivered with QoS {ncmd.qos}",
        )
        try:
            payload = decode_payload(ncmd.payload)
        except DecodeError as exc:
            report.check(False, "NCMD payload decodes with the Tahu schema", str(exc))
        else:
            report.check(True, "NCMD payload decodes with the Tahu schema", describe(payload))
            metric = find_metric(payload, METRIC_REBIRTH)
            report.check(
                metric is not None and metric_value(metric) is True,
                f"NCMD payload carries the {METRIC_REBIRTH} metric set to true",
                describe(payload),
            )

    # -- DCMD ---------------------------------------------------------------
    dcmd_topic = device_topic("DCMD")
    dcmd_metrics = [
        build_metric(name=METRIC_DEVICE_REBIRTH, datatype=DATATYPE_BOOLEAN, boolean_value=True),
    ]
    report.note(f"publishing {dcmd_topic}")
    host.publish(dcmd_topic, build_payload(seq=0, metrics=dcmd_metrics), qos=1)
    received = node.collector.wait_until(
        lambda items: any(item.topic == dcmd_topic for item in items),
        time.monotonic() + config.timeout,
    )
    if report.check(received, f"DCMD reaches the edge node on {dcmd_topic}"):
        dcmd = next(item for item in node.collector.snapshot() if item.topic == dcmd_topic)
        report.check(
            dcmd.qos == 1,
            "the edge node receives the DCMD at QoS 1",
            f"delivered with QoS {dcmd.qos}",
        )
        try:
            payload = decode_payload(dcmd.payload)
        except DecodeError as exc:
            report.check(False, "DCMD payload decodes with the Tahu schema", str(exc))
        else:
            report.check(True, "DCMD payload decodes with the Tahu schema", describe(payload))
            metric = find_metric(payload, METRIC_DEVICE_REBIRTH)
            report.check(
                metric is not None and metric_value(metric) is True,
                f"DCMD payload carries the {METRIC_DEVICE_REBIRTH} metric set to true",
                describe(payload),
            )

    # -- the Will of the Edge Node -----------------------------------------
    # Dropping the connection without a DISCONNECT makes the broker publish
    # the Will, which is how an NDEATH reaches a Host Application when the
    # Edge Node dies unexpectedly.
    report.note("dropping the connection of the edge node so the broker publishes the Will")
    node.kill_connection()

    received = host.collector.wait_until(
        lambda items: has_type(items, "NDEATH", edge),
        time.monotonic() + config.timeout,
    )
    if report.check(received, f"NDEATH reaches the host application on {node_ndeath_topic}"):
        ndeath = find_received(host.collector.snapshot(), message_type="NDEATH", edge_node_id=edge)
        assert ndeath is not None
        try:
            payload = decode_payload(ndeath.payload)
        except DecodeError as exc:
            report.check(False, "NDEATH payload decodes with the Tahu schema", str(exc))
        else:
            report.check(True, "NDEATH payload decodes with the Tahu schema", describe(payload))
            validate_ndeath(report, "NDEATH", payload, expected_bd_seq=bd_seq)
            validate_delivery(report, "NDEATH", ndeath, qos=1)


# ---------------------------------------------------------------------------
# observe mode
# ---------------------------------------------------------------------------


def run_observe(host: SparkplugHost, report: Report) -> None:
    """Observe what a real gateway publishes and assert the flow."""
    config = host.config
    group, edge = config.group_id, config.edge_node_id

    required = ["NBIRTH"]
    if config.expect_device:
        # A Device only exists once a sensor node has been discovered through
        # ESP-NOW, so a gateway without nodes can never publish these.
        required.extend(["DBIRTH", "DDATA"])
    if config.expect_ddeath:
        required.append("DDEATH")
    if config.expect_ndata:
        required.append("NDATA")

    report.note(
        f"observing the Sparkplug namespace of group '{group}' for edge node "
        f"'{edge}' (required messages: {', '.join(required)})"
    )

    deadline = time.monotonic() + config.timeout
    host.collector.wait_until(
        lambda items: all(has_type(items, message_type, edge) for message_type in required),
        deadline,
    )

    messages = host.collector.snapshot()
    report.note(f"received {len(messages)} message(s) in total")

    tracker = SeqTracker()
    registry = AliasRegistry()
    nbirth_bd_seq: int | None = None
    ndata_aliases: set[int] = set()
    declared_aliases: set[int] = set()

    for item in messages:
        try:
            topic = parse_topic(item.topic)
        except ValueError as exc:
            report.check(False, f"received topic '{item.topic}' is a valid Sparkplug topic", str(exc))
            continue

        label = topic.message_type
        if topic.message_type in ("NCMD", "DCMD"):
            # Commands are inputs for the node; their payload shape is the
            # gateway's business, not a conformance requirement of the flow.
            continue

        try:
            payload = decode_payload(item.payload)
        except DecodeError as exc:
            report.check(False, f"{label} payload decodes with the Tahu schema", str(exc))
            continue

        report.check(True, f"{label} payload decodes with the Tahu schema", describe(payload))

        if label == "NDEATH":
            validate_ndeath(report, label, payload, expected_bd_seq=nbirth_bd_seq)
            validate_delivery(report, label, item, qos=1)
            continue

        if label == "DDEATH":
            validate_ddeath(report, label, payload)
            validate_delivery(report, label, item, qos=0)
            tracker.check(report, label, edge, payload.seq, item.payload)
            continue

        if label in ("NBIRTH", "DBIRTH"):
            validate_birth(
                report,
                label,
                payload,
                edge_node_id=edge,
                require_bd_seq=(label == "NBIRTH"),
                require_rebirth_metric=(label == "NBIRTH"),
                registry=registry,
            )
            validate_delivery(report, label, item, qos=0)
            if label == "NBIRTH":
                bd_seq = find_bd_seq(payload)
                report.check(
                    bd_seq is not None,
                    f"NBIRTH payload carries the {METRIC_BD_SEQ} metric",
                    "bdSeq marks the birth generation of the session",
                )
                if bd_seq is not None and nbirth_bd_seq is None:
                    nbirth_bd_seq = bd_seq
                tracker.check(report, label, edge, payload.seq, item.payload, is_birth=True)
            else:
                tracker.check(report, label, edge, payload.seq, item.payload)
            declared_aliases.update(registry.aliases_of(edge).keys())
            continue

        if label in ("NDATA", "DDATA"):
            validate_data(report, label, payload)
            validate_delivery(report, label, item, qos=0)
            tracker.check(report, label, edge, payload.seq, item.payload)
            ndata_aliases.update(metric.alias for metric in payload.metrics)
            continue

        report.note(f"ignoring unsupported message type {label} on {item.topic}")

    # Every alias published in a DATA message must have been bound to a name
    # by a BIRTH message, otherwise a Host Application cannot resolve it.
    undeclared = sorted(ndata_aliases - declared_aliases)
    if undeclared:
        report.note(
            f"DATA aliases without a BIRTH declaration: {undeclared} "
            "(the ESP-NOW discovery frame does not carry the value count, so the "
            "gateway cannot know in advance which Custom slots a device will use)"
        )
    else:
        report.note("every alias published in a DATA message was declared by a BIRTH")

    for message_type in required:
        present = has_type(messages, message_type, edge)
        report.check(
            present,
            f"{message_type} was published by the gateway",
            "" if present else f"expected within {config.timeout:.0f}s",
        )

    # -- optional rebirth probe ---------------------------------------------
    if config.expect_rebirth:
        ncmd_topic = f"{SPARKPLUG_VERSION}/{group}/NCMD/{edge}"
        ncmd_metrics = [
            build_metric(name=METRIC_REBIRTH, datatype=DATATYPE_BOOLEAN, boolean_value=True),
        ]
        report.note(f"publishing a rebirth request on {ncmd_topic}")
        host.publish(ncmd_topic, build_payload(seq=0, metrics=ncmd_metrics), qos=1)

        def second_nbirth(items: list[Received]) -> bool:
            count = 0
            for item in items:
                try:
                    topic = parse_topic(item.topic)
                except ValueError:
                    continue
                if topic.message_type == "NBIRTH" and topic.edge_node_id == edge:
                    count += 1
            return count >= 2

        host.collector.wait_until(second_nbirth, time.monotonic() + config.timeout)
        births = [
            item
            for item in host.collector.snapshot()
            if (lambda t: t is not None and t.message_type == "NBIRTH" and t.edge_node_id == edge)(
                _safe_parse(item.topic)
            )
        ]
        report.check(
            len(births) >= 2,
            "the gateway republishes the NBIRTH after a rebirth request",
            f"observed {len(births)} NBIRTH message(s)",
        )
        if len(births) >= 2 and nbirth_bd_seq is not None:
            try:
                rebirth_payload = decode_payload(births[-1].payload)
            except DecodeError as exc:
                report.check(False, "the rebirth NBIRTH decodes with the Tahu schema", str(exc))
            else:
                rebirth_bd_seq = find_bd_seq(rebirth_payload)
                report.check(
                    rebirth_bd_seq == nbirth_bd_seq,
                    "the rebirth keeps the bdSeq of the session",
                    f"first={nbirth_bd_seq} rebirth={rebirth_bd_seq}",
                )


def _safe_parse(topic: str) -> SparkplugTopic | None:
    """Parse a topic, returning ``None`` when it is not a Sparkplug topic."""
    try:
        return parse_topic(topic)
    except ValueError:
        return None


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def env_float(name: str, default: float) -> float:
    """Read a float from the environment, falling back to ``default``."""
    raw = os.environ.get(name)
    if raw is None or raw.strip() == "":
        return default
    try:
        return float(raw)
    except ValueError:
        raise HarnessError(f"{name} must be a number, got {raw!r}") from None


def env_bool(name: str, default: bool = False) -> bool:
    """Read a boolean from the environment, falling back to ``default``."""
    raw = os.environ.get(name)
    if raw is None or raw.strip() == "":
        return default
    return raw.strip().lower() in ("1", "true", "yes", "on")


class ArgumentParser(argparse.ArgumentParser):
    """Argument parser that reports usage errors as harness errors.

    argparse exits with code 2 on a usage error, which would be confused with
    an assertion failure. Usage errors are harness errors here, so they exit
    with code 1.
    """

    def error(self, message: str) -> "NoReturn":  # type: ignore[override]
        self.print_usage(sys.stderr)
        print(f"[FAIL] harness error -- {message}", file=sys.stderr)
        raise SystemExit(EXIT_HARNESS_ERROR)


def build_parser() -> ArgumentParser:
    parser = ArgumentParser(
        prog="conformance.py",
        description=(
            "Sparkplug B v3.0.0 conformance harness. Acts as a Sparkplug Host "
            "Application against an MQTT 3.1.1 broker and asserts the "
            "NBIRTH/DBIRTH/DDATA/NDATA/NDEATH/DDEATH and NCMD/DCMD message flow."
        ),
        epilog=(
            "examples:\n"
            "  conformance.py --mode simulate\n"
            "  conformance.py --mode observe --timeout 60\n"
            "  conformance.py --broker mqtt://host.docker.internal:1883 "
            "--mode observe\n\n"
            "environment: MODE, SPARKPLUG_BROKER_URI, SPARKPLUG_GROUP_ID,\n"
            "SPARKPLUG_EDGE_NODE_ID, SPARKPLUG_DEVICE_ID, SPARKPLUG_TIMEOUT,\n"
            "SPARKPLUG_EXPECT_DDEATH, SPARKPLUG_EXPECT_NDATA,\n"
            "SPARKPLUG_EXPECT_REBIRTH\n"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--mode",
        choices=("simulate", "observe"),
        default=os.environ.get("MODE", "simulate"),
        help="simulate publishes a synthetic flow; observe only watches the gateway",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=env_float("SPARKPLUG_TIMEOUT", 30.0),
        help="seconds to wait for the broker and for messages (default: 30)",
    )
    parser.add_argument(
        "--broker",
        "--broker-uri",
        dest="broker_uri",
        default=os.environ.get("SPARKPLUG_BROKER_URI", "mqtt://broker:1883"),
        help="broker URI, e.g. mqtt://broker:1883 (default: mqtt://broker:1883)",
    )
    parser.add_argument(
        "--group-id",
        default=os.environ.get("SPARKPLUG_GROUP_ID", "iiot-kit"),
        help="Sparkplug group id (default: iiot-kit)",
    )
    parser.add_argument(
        "--edge-node-id",
        default=os.environ.get("SPARKPLUG_EDGE_NODE_ID", "GW-TEST01"),
        help="edge node id (default: GW-TEST01)",
    )
    parser.add_argument(
        "--device-id",
        default=os.environ.get("SPARKPLUG_DEVICE_ID", "DEV-TEST01"),
        help="device id used by the simulated flow (default: DEV-TEST01)",
    )
    parser.add_argument(
        "--expect-device",
        action=argparse.BooleanOptionalAction,
        default=env_bool("SPARKPLUG_EXPECT_DEVICE", True),
        help=(
            "in observe mode, also require the Device messages DBIRTH and DDATA "
            "(default: enabled; disable it for a gateway that has not discovered "
            "any sensor node yet)"
        ),
    )
    parser.add_argument(
        "--expect-ddeath",
        action="store_true",
        default=env_bool("SPARKPLUG_EXPECT_DDEATH"),
        help="in observe mode, also require a DDEATH message",
    )
    parser.add_argument(
        "--expect-ndata",
        action="store_true",
        default=env_bool("SPARKPLUG_EXPECT_NDATA"),
        help="in observe mode, also require an NDATA message",
    )
    parser.add_argument(
        "--expect-rebirth",
        action="store_true",
        default=env_bool("SPARKPLUG_EXPECT_REBIRTH"),
        help="in observe mode, request a rebirth and require a second NBIRTH",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="enable debug logging",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)-7s %(message)s",
        datefmt="%H:%M:%S",
    )

    config = Config(
        mode=args.mode,
        broker_uri=args.broker_uri,
        group_id=args.group_id,
        edge_node_id=args.edge_node_id,
        device_id=args.device_id,
        timeout=args.timeout,
        expect_device=args.expect_device,
        expect_ddeath=args.expect_ddeath,
        expect_ndata=args.expect_ndata,
        expect_rebirth=args.expect_rebirth,
    )

    report = Report()
    host = SparkplugHost(config, report)
    node = SparkplugNode(config, report)

    try:
        host.connect()
        host.subscribe_namespace()
        if config.mode == "simulate":
            run_simulate(host, node, report)
        else:
            run_observe(host, report)
    except HarnessError as exc:
        print(f"[FAIL] harness error -- {exc}", flush=True)
        print(report.summary(), flush=True)
        return EXIT_HARNESS_ERROR
    except KeyboardInterrupt:
        print("[FAIL] interrupted -- the run was cancelled", flush=True)
        return EXIT_HARNESS_ERROR
    finally:
        host.disconnect()
        node.disconnect()

    print()
    print(report.summary())
    return EXIT_OK if report.failures == 0 else EXIT_ASSERTION_FAILURE


if __name__ == "__main__":
    sys.exit(main())

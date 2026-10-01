# sparkplug_b

Component for the gateway's optional Sparkplug B v3.0.0 mode (issue #11).

**Status:** stub (commit 1). Real implementation arrives in commits 3–11.

When `CONFIG_SPARKPLUG_B_ENABLED=y`, the gateway acts as a Sparkplug B Edge
of Network (EoN) Node. Each sensor node known via ESP-NOW is represented as
a Device on the broker. Birth / Data / Death certificates, LWT, and the seq /
bdSeq state machine are owned by this component.

When the flag is off (default), this component compiles to an empty object and
`mqtt_bridge_start()` stays a no-op (Fase 4 placeholder).

See `odd/tasks/issue-11-sparkplug-b/design.md` for the full design.
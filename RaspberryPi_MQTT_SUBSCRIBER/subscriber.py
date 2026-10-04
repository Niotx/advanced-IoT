"""Receive MQTT telemetry and device status from the ESP32 DHT22 project.

Run on the Raspberry Pi after configuring Mosquitto. This lesson intentionally
prints messages only: logging, storage, dashboards, and TLS come later.

Requirements: pip install "paho-mqtt>=2.1,<3"
"""

import getpass
import json
import math
import os
import sys

import paho.mqtt.client as mqtt

BROKER_HOST = os.environ.get("MQTT_HOST", "127.0.0.1")
BROKER_PORT = int(os.environ.get("MQTT_PORT", "1883"))
MQTT_USERNAME = os.environ.get("MQTT_USERNAME", "")
TOPIC_TELEMETRY = "iot/room-01/telemetry"
TOPIC_STATUS = "iot/room-01/status"


def on_connect(client, userdata, flags, reason_code, properties):
    """Subscribe on every successful connection, including reconnections."""
    if reason_code != 0:
        print(f"MQTT connection rejected: {reason_code}", flush=True)
        return

    print(f"Connected to {BROKER_HOST}:{BROKER_PORT}", flush=True)
    client.subscribe([(TOPIC_TELEMETRY, 0), (TOPIC_STATUS, 0)])
    print(f"Subscribed: {TOPIC_TELEMETRY}, {TOPIC_STATUS}", flush=True)


def on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
    if reason_code != 0:
        print(f"MQTT connection lost ({reason_code}); waiting to reconnect...", flush=True)


def on_message(client, userdata, message):
    """Interpret status as text and telemetry as JSON."""
    try:
        payload = message.payload.decode("utf-8")
    except UnicodeDecodeError:
        print(f"Non-UTF-8 message ignored: {message.topic}", flush=True)
        return

    if message.topic == TOPIC_STATUS:
        marker = " (retained)" if message.retain else ""
        print(f"[STATUS]{marker} {payload}", flush=True)
        return

    if message.topic != TOPIC_TELEMETRY:
        return

    try:
        data = json.loads(payload)
        device_id = data["device_id"]
        temperature = float(data["temperature"])
        humidity = float(data["humidity"])
        uptime_ms = int(data["uptime_ms"])
        if not isinstance(device_id, str) or not all(
            math.isfinite(value) for value in (temperature, humidity)
        ):
            raise ValueError("invalid telemetry field")
    except (json.JSONDecodeError, KeyError, ValueError, TypeError) as exc:
        print(f"Invalid telemetry ({exc}): {payload}", flush=True)
        return

    print(
        f"[TELEMETRY] {device_id}: {temperature:.1f} C, "
        f"{humidity:.1f} %, uptime={uptime_ms} ms",
        flush=True,
    )


def main():
    if not MQTT_USERNAME:
        print("Set MQTT_USERNAME first (example: export MQTT_USERNAME=iot_device)")
        return 2

    password = os.environ.get("MQTT_PASSWORD")
    if password is None:
        password = getpass.getpass("MQTT password (input hidden): ")

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"rpi-course-subscriber-{os.getpid()}",
        protocol=mqtt.MQTTv311,
    )
    client.username_pw_set(MQTT_USERNAME, password)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    try:
        client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
        print("Waiting for messages. Press Ctrl+C to stop.", flush=True)
        client.loop_forever()
    except (OSError, ValueError) as exc:
        print(f"Cannot connect to MQTT broker: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nStopped by user.")
    finally:
        client.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())

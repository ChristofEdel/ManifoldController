#!/usr/bin/env python3

import argparse
import re
import time

import paho.mqtt.client as mqtt


def make_subscription_filter(pattern: str) -> str:
    """Create an MQTT subscription filter broad enough to receive all matches."""
    levels = pattern.split("/")

    for i, level in enumerate(levels):
        if "*" in level:
            if i == 0:
                return "#"
            return "/".join(levels[:i] + ["#"])

        if level == "#":
            return "/".join(levels[:i + 1])

    return pattern


def topic_matches(pattern: str, topic: str) -> bool:
    """
    Match a topic against a pattern supporting:
      *  zero or more characters
      +  exactly one MQTT topic level
      #  zero or more remaining MQTT topic levels
    """
    regex = re.escape(pattern)
    regex = regex.replace(r"\*", ".*")
    regex = regex.replace(r"\+", "[^/]+")
    regex = regex.replace(r"\#", ".*")
    return re.fullmatch(regex, topic) is not None


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Delete retained MQTT messages matching a topic pattern."
    )
    parser.add_argument("positional_topic", nargs="?", help="MQTT topic/pattern")
    parser.add_argument("-topic", dest="topic", help="MQTT topic/pattern")
    parser.add_argument(
        "-server",
        default="mqtt.internal",
        help="MQTT server (default: mqtt.internal)",
    )
    parser.add_argument(
        "-do",
        action="store_true",
        help="actually delete matching retained messages",
    )

    args = parser.parse_args()

    topic_pattern = args.topic or args.positional_topic

    if not topic_pattern:
        parser.error("an MQTT topic must be specified")

    if args.topic and args.positional_topic:
        parser.error("specify the topic either positionally or with -topic, not both")

    subscription = make_subscription_filter(topic_pattern)

    print(f"Pattern:      {topic_pattern}")
    print(f"Subscription: {subscription}")

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            client.subscribe(subscription)
        else:
            print(f"MQTT connection failed: {reason_code}")

    def on_message(client, userdata, message):
        if message.retain and topic_matches(topic_pattern, message.topic):
            if args.do:
                print(f"Deleting retained message: {message.topic}")
                client.publish(message.topic, payload=None, qos=1, retain=True)
            else:
                print(f"Would delete retained message: {message.topic}")

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(args.server)
    client.loop_start()

    try:
        time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        client.loop_stop()
        client.disconnect()

    if not args.do:
        print("use -do to execute")


if __name__ == "__main__":
    main()

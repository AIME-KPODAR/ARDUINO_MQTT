import csv
from datetime import datetime
import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"
DERNIERES = {}

def on_connect(client, userdata, flags, reason_code, properties):
    client.subscribe("monprojet123/serre/#")

def on_message(client, userdata, msg):
    nom = msg.topic.split("/")[-1]
    DERNIERES[nom] = msg.payload.decode()
    print(nom, "=", DERNIERES[nom])
    if nom == "humidite":  # dernier message d'un cycle de mesure
        with open("serre.csv", "a", newline="") as f:
            csv.writer(f, delimiter=";").writerow([
                datetime.now().isoformat(timespec="seconds"),
                DERNIERES.get("temperature"), DERNIERES.get("humidite"),
                DERNIERES.get("ventilateur"), DERNIERES.get("mode"),
            ])

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, 1883)
client.loop_forever()
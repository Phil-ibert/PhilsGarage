# PhilsGarage — passerelle LoRa du garage pour Home Assistant (ESPHome)

Passerelle ESP32 + modem Semtech qui reçoit l'émetteur LoRa du garage et publie ses
mesures dans Home Assistant via l'**API native** (pas de MQTT).

Elle parle exactement le protocole de l'émetteur Arduino (PhilsCryptoLoRa :
AES-256-ECB + CRC32, trame `#T:..;H:..;L:..;E:..;P:..$`), y compris l'émetteur
d'origine, sans le reflasher.

## Utilisation depuis Home Assistant

1. Dans l'add-on ESPHome, crée `lora-garage.yaml` avec le contenu du fichier
   [`lora-garage.yaml`](lora-garage.yaml) de ce dépôt. C'est le seul fichier à copier :
   la configuration et le composant `phils_lora` sont téléchargés depuis GitHub.
2. Ajoute à ton `secrets.yaml` ESPHome les clés de [`secrets.example.yaml`](secrets.example.yaml).
   `phils_lora_aes_key` = le contenu de `aes_key[32]` de `PhilsCryptoLoRa.h`
   (le format `0x.., 0x.., ...` du tableau C est accepté tel quel).
3. Premier flash en USB, ensuite en OTA.
4. Supprime l'ancien appareil MQTT « Garage LoRa » (Paramètres > Appareils) : ça efface
   aussi ses messages de découverte retenus sur le broker.

Le dépôt doit être accessible par ESPHome (public, ou URL avec jeton pour un dépôt privé).
ESPHome relit GitHub au plus une fois par jour : pour prendre en compte un push tout de
suite, mets `philsgarage_refresh: 0s` dans ton `lora-garage.yaml`, compile, puis remets `1d`.

## Changer de carte / de puce Semtech

Dans ton `lora-garage.yaml` :

```yaml
    files:
      - packages/lora-garage.yaml
      - packages/radio/sx127x.yaml   # SX1276/77/78/79 (Ra-02, RFM9x, TTGO LoRa32...)
      # - packages/radio/sx126x.yaml # SX1261/62/68, LLCC68 (E22, Heltec V3...)
```

puis redéfinis seulement ce qui change dans `substitutions:` (broches, puissance...).
Fréquence, SF, BW, CR, préambule et sync word doivent rester identiques à l'émetteur.

## Arborescence

| Chemin | Rôle |
|---|---|
| `lora-garage.yaml` | fichier à placer dans Home Assistant |
| `packages/lora-garage.yaml` | configuration de la passerelle (entités, Wi-Fi, API...) |
| `packages/radio/*.yaml` | une puce Semtech par fichier, câblage et paramètres radio |
| `components/phils_lora/` | composant externe : chiffrement, CRC, décodage, commandes |
| `secrets.example.yaml` | clés à ajouter à ton `secrets.yaml` |

## Entités

| Entité | Source |
|---|---|
| Température, Humidité | champs `T`, `H` (les échecs DHT22 -273 / -1 sont filtrés) |
| Énergie (kWh, `total_increasing`, utilisable dans le tableau Énergie) | champ `E` |
| Puissance (W) | champ `W`, envoyé seulement par Transmitter_v2 |
| Lumière garage (switch) | commande `S`, état `L` |
| Compteur - valeur / appliquer / recopier | commande `R` |
| Liaison LoRa, RSSI, SNR, paquets valides / rejetés | diagnostic |
| Heap libre / plus grand bloc / minimum | surveillance mémoire de la passerelle |

Un nouveau champ ajouté côté émetteur se récupère sans toucher au C++ :

```yaml
sensor:
  - platform: phils_lora
    field: D        # ex. "D:18.25" envoyé par l'émetteur
    name: Température DS18B20
```

## Composant `phils_lora`

- `phils_lora:` — `key` (clé AES-256), `on_transmit` (reçoit `x` = paquet chiffré à
  passer au `send_packet` du modem), `on_message` (`message`, `rssi`, `snr`).
- `phils_lora.send` — action, `payload: "#S:1$"` (texte clair, le CRC et le chiffrement
  sont ajoutés).
- `sensor` : `type: field` (défaut, avec `field:`), `rssi`, `snr`, `valid_packets`,
  `rejected_packets`.
- `binary_sensor` : `type: field` (défaut), `link` (`timeout:`).
- `text_sensor` : `type: last_message` ou `last_error`.
- `switch` : `command_field`, `state_field`, `repeat`, `repeat_interval`, `confirm_delay`.
  Aucune commande n'est envoyée au démarrage de la passerelle.

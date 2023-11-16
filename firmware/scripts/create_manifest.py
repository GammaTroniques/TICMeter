import csv
import json
import re

csv_file = "../partitions.csv"
json_file = "../build/manifest.json"

fileURL="https://github.com/GammaTroniques/TICMeter/releases/latest/download/"

toFill = {
    "otadata": "ota_data_initial.bin",
    "storage": "storage.bin",
    "ota_0": "TICMeter.bin"
}


offset = []
# Lire le fichier CSV
with open(csv_file, mode='r') as file:
    csv_reader = csv.reader(file, delimiter=',')
    # check if comment
    next(csv_reader)
    next(csv_reader)
    next(csv_reader)
    
    for row in csv_reader:
        # remove spaces
        row = [x.strip(' ') for x in row]
        if row[0] in toFill:
            offset.append(int(row[3], 16))
            print(f"{row[0]}: {row[3]}")
        
# Construire la structure JSON avec les nouvelles valeurs d'offset
output_data = {
    "name": "ESP Linky TIC",
    "version": "2.0",
    "home_assistant_domain": "esphome",
    "funding_url": "https://esphome.io/guides/supporters.html",
    "new_install_prompt_erase": True,
    "builds": [
        {
            "chipFamily": "ESP32-C6",
            "parts": [
                {"path": fileURL + "bootloader.bin", "offset": 0},
                {"path": fileURL + "partition-table.bin", "offset": 32768},
                {"path": fileURL + "otadata_initial.bin", "offset": offset[0]},
                {"path": fileURL + "storage.bin", "offset": offset[1]},
                {"path": fileURL + "TICMeter.bin", "offset": offset[2]}
            ]
        }
    ]
}

# Écrire le fichier JSON résultant
with open(json_file, 'w') as json_output:
    json.dump(output_data, json_output, indent=2)

print(f"Le fichier JSON a été généré avec succès : {json_file}")

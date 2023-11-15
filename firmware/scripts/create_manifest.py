import json

url = "firmware/"

def convert_csv_to_json(csv_file_path, json_file_path):
    builds = [{
        "chipFamily": "ESP32-C6",
        "parts": [{
            "path": url + "bootloader.bin",
            "offset": 0
        },        
        {
            "path": url + "partition-table.bin",
            "offset": 32768
        }
      ]
    }]


    with open(csv_file_path, 'r') as csv_file:
        # Skip the header line
        next(csv_file)
        next(csv_file)
        next(csv_file)
        
        for line in csv_file:
            parts = line.strip().split(',')
            print(parts)
            name, type, subtype, offset_hex, size, flag = parts

            offset = int(offset_hex, 16)

            build_part = {
                "path": f"firmware/{name}.bin",
                "offset": offset
            }

            builds[0]["parts"].append(build_part)

    result = {
        "name": "ESP Linky TIC",
        "version": "2.0",
        "home_assistant_domain": "esphome",
        "funding_url": "https://esphome.io/guides/supporters.html",
        "new_install_prompt_erase": True,
        "builds": builds
    }

    with open(json_file_path, 'w') as json_file:
        json.dump(result, json_file, indent=2)

if __name__ == "__main__":
    csv_file_path = "../partitions.csv"
    json_file_path = "../build/manifest.json"
    convert_csv_to_json(csv_file_path, json_file_path)

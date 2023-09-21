
import sys
import pandas as pd
import serial
import pprint
import json
import os
import io


total = len(sys.argv)
cmdargs = str(sys.argv)

if total != 3:
    print("You need to pass the arguments: <COMx> <excel file>")
    sys.exit(1)
# read by default 1st sheet of an excel file
tuyaKeys = pd.read_excel(sys.argv[2])



SAVE_PATH = 'save.json'

def startupCheck():
    if os.path.isfile(SAVE_PATH) and os.access(SAVE_PATH, os.R_OK):
        # checks if file exists
        print ("File exists and is readable")
        f = open(SAVE_PATH)
        save = json.load(f)
        return save
    else:
        print ("Either file is missing or is not readable, creating file...")
        with io.open(SAVE_PATH, 'w') as db_file:
            newFile = {
                "tuyaIndex": 0,
                "productID": ""
            }
            db_file.write(json.dumps(newFile))
            return newFile
            
            
def responseToDict(response):
    # App version: 2.0.1
    # Git commit: cc0aa6f+
    # Git tag:
    # Git branch: dev
    # Build time: 2023-09-21 09:26:33

    # create a dictionary
    dict = {}
    # split response by \n
    lines = response.split(b"\n")
    # iterate over lines
    for line in lines:
        # split line by :
        key_value = line.split(b":")
        # add key and value to dictionary
        if len(key_value) <= 1:
            continue
        dict[key_value[0].decode().replace('\r', '')] = key_value[1].decode().replace('\r', '').replace(' ', '')
    return dict


def textEncode(text):
    return text.encode('utf-8')

# open serial port

save = startupCheck()
ser = serial.Serial(sys.argv[1], 115200, timeout=1)


def sendTuya():
    tuyaIndex = save["tuyaIndex"]
    productID = save["productID"]
    try:
        uuid = tuyaKeys["uuid"][tuyaIndex]
        key = tuyaKeys["key"][tuyaIndex]
    except:
        print("Error: No more keys available: index %d" % tuyaIndex)
        return True
    cmd = "set-tuya %s %s %s\r\n" % (productID, uuid, key)
    print(cmd)
    ser.write(textEncode(cmd))
    with open(SAVE_PATH, 'w') as f:
        json.dump(save, f)

    # read response
    response = ser.read_until(b"\02")
    response = ser.read_until(b"\03")
    dict = responseToDict(response)
    pprint.pprint(dict)

    # check if value is set
    error = False
    if dict["Device Auth"] != key:
        print("Error: Auth key not valid")
        error = True
    if dict["Device UUID"] != uuid:
        print("Error: UUID not valid")
        error = True
    if dict["Product ID"] != productID:
        print("Error: Product ID not valid")
        error = True
    return error


if ser.isOpen():
    print(ser.name + ' is open...')
    # send command to device
    ser.write("info\r\n".encode())
    # read response
    response = ser.read_until(b"\02")
    response = ser.read_until(b"\03")
    # remove \02 and \03 from response
    response = response[0:-1]
    dict = responseToDict(response)
    pprint.pprint(dict)
    
    nTry = 0
    while sendTuya() and nTry < 3:
        print("Try again...")
        nTry += 1
    if nTry >= 3:
        print("Error: Too many tries, please check your device")
    else:
        save["tuyaIndex"] += 1
        with open(SAVE_PATH, 'w') as f:
            json.dump(save, f)
        print("Done!")
        
        
else:
    print("Error: Can't open serial port")


   
    
    
    
        
    
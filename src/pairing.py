import asyncio
from bleak import BleakClient, BleakError
import aiohttp
import json

# BLE cihazının MAC adresi
BLE_DEVICE_ADDRESS = "A0:A3:B3:97:3B:1E"  # ESP32'nin MAC adresi
# BLE karakteristik UUID'leri
WRITE_CHARACTERISTIC_UUID = "00005555-EAD2-11E7-80C1-9A214CF093AE"
READ_CHARACTERISTIC_UUID = "00005555-EAD2-11E7-80C1-9A214CF093AE"
port = 80 

async def pair_and_send_credentials(client, ssid, password):
    try:
        await client.pair()
        if client.is_connected:
            print("Connected to BLE device")
            await asyncio.sleep(3)
            
            credentials = f'{{"ssidPrim":"{ssid}","pwPrim":"{password}","ssidSec":"{ssid}","pwSec":"{password}"}}'
            await client.write_gatt_char(WRITE_CHARACTERISTIC_UUID, credentials.encode('utf-8'))
            print(f"Sent credentials: SSID={ssid}, Password={password}")
            
            await asyncio.sleep(3)
            ip_address = await client.read_gatt_char(READ_CHARACTERISTIC_UUID)
            ip_address = ip_address.decode()
            print(f"Received IP Address: {ip_address}")
            
            return ip_address
        else:
            print("Failed to connect to BLE device")
            return None
    except BleakError as e:
        print(f"An error occurred: {e}")
        return None

async def make_get_request(ip_address):
    if ip_address:
        print(f"Attempting to make HTTP get request to http://{ip_address}:{port}/get?data=beyza")
        url = f"http://{ip_address}:{port}/get?data=beyza"
        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(url) as response:
                    print("Status:", response.status)
                    html = await response.text()
                    print("Body:", html[:15], "...")
        except aiohttp.ClientError as e:
            print(f"HTTP request error: {e}")
    else:
        print("No IP address to send HTTP request")

async def make_post_request(ip_address):
    if ip_address:
        print(f"Attempting to make HTTP post request to http://{ip_address}:{port}/post")
        url = f"http://{ip_address}:{port}/post"
        data = {'data': 'Your data here'}
    
        async with aiohttp.ClientSession() as session:
            async with session.post(url, data=data) as response:
                if response.headers['Content-Type'] == 'application/json':
                    json_response = await response.json()
                    print("json: ",json_response)
                elif response.headers['Content-Type'] == 'text/plain':
                    text_response = await response.text()
                    print("text: ",text_response)
                else:
                    print(f"Unexpected content type: {response.headers['Content-Type']}")
    else:
        print("No IP address to send HTTP request")

async def main(ssid, password):
    i=1
    async with BleakClient(BLE_DEVICE_ADDRESS) as client:
        ip_address = await pair_and_send_credentials(client, ssid, password)
        if(i==0):
            await make_get_request(ip_address)
        if(i==1):
            await make_post_request(ip_address)

# Kullanıcı bilgileri
ssid = "GMF"
password = "Gmf463.."

# Mevcut event loop'u al ve fonksiyonu çalıştır
loop = asyncio.get_event_loop()
loop.run_until_complete(main(ssid, password))
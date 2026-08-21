from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/sensor', methods=['POST'])
def receive_sensor_data():
    # 1. Parse incoming JSON payload from ESP32
    data = request.get_json()
    
    if not data:
        return jsonify({"status": "error", "message": "No JSON payload received"}), 400

    # 2. Print data cleanly to the console
    print("\n================ INCOMING TELEMETRY ================")
    print(f"Temperature : {data.get('temp')} °C")
    print(f"Is Raining  : {data.get('raining')}")
    print("---------------------------------------------------")
    print(f"Mains       : {data['mains']['v']}V | {data['mains']['i']}mA | {data['mains']['p']}W")
    print(f"Charge      : {data['charge']['v']}V | {data['charge']['i']}mA | {data['charge']['p']}W")
    print(f"Motor       : {data['motor']['v']}V | {data['motor']['i']}mA | {data['motor']['p']}W")
    print("---------------------------------------------------")
    print(f"Lights      : {data['lights']['v']}V | {data['lights']['i']}mA | {data['lights']['p']}W")
    print(f"USB         : {data['usb']['v']}V | {data['usb']['i']}mA | {data['usb']['p']}W")
    print(f"Backup      : {data['backup']['v']}V | {data['backup']['i']}mA | {data['backup']['p']}W")
    print("===================================================\n")

    # 3. Respond back to ESP32 with HTTP 200 OK
    return jsonify({"status": "success", "message": "Data received"}), 200

if __name__ == '__main__':
    # host='0.0.0.0' allows external devices on your local network (like ESP32) to connect
    app.run(host='0.0.0.0', port=8000, debug=True)
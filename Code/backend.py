from flask import Flask, request, jsonify
import requests
from datetime import datetime, timedelta
from flask_cors import CORS 
from datetime import datetime
import json
import sqlite3
import os

app = Flask(__name__)
CORS(app)

# ==================== DATABASE SETUP ====================
DATABASE = "system_data.db"

def init_database():
    """Initialize SQLite database"""
    if not os.path.exists(DATABASE):
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        
        # Create table for sensor data
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS sensor_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                l1 BOOLEAN,
                l2 BOOLEAN,
                l3 BOOLEAN,
                l4 BOOLEAN,
                temp REAL,
                raining BOOLEAN,
                mains_v REAL,
                mains_i REAL,
                mains_p REAL,
                charger_v REAL,
                charger_i REAL,
                charger_p REAL,
                motor_v REAL,
                motor_i REAL,
                motor_p REAL,
                lights_v REAL,
                lights_i REAL,
                lights_p REAL,
                usb_v REAL,
                usb_i REAL,
                usb_p REAL,
                backup_v REAL,
                backup_i REAL,
                backup_p REAL
            )
        ''')
        
        conn.commit()
        conn.close()
        print("✓ Database initialized")

# ==================== HELPER FUNCTIONS ====================
def store_sensor_data(data):
    """Store received sensor data in SQLite"""
    try:
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        
        timestamp = datetime.now().isoformat()
        
        cursor.execute('''
            INSERT INTO sensor_data (
                timestamp, l1, l2, l3, l4, temp, raining,
                mains_v, mains_i, mains_p,
                charger_v, charger_i, charger_p,
                motor_v, motor_i, motor_p,
                lights_v, lights_i, lights_p,
                usb_v, usb_i, usb_p,
                backup_v, backup_i, backup_p
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (
            timestamp,
            data.get('l1'),
            data.get('l2'),
            data.get('l3'),
            data.get('l4'),
            data.get('temp'),
            data.get('raining'),
            data['mains'].get('v'),
            data['mains'].get('i'),
            data['mains'].get('p'),
            data['charger'].get('v'),
            data['charger'].get('i'),
            data['charger'].get('p'),
            data['motor'].get('v'),
            data['motor'].get('i'),
            data['motor'].get('p'),
            data['lights'].get('v'),
            data['lights'].get('i'),
            data['lights'].get('p'),
            data['usb'].get('v'),
            data['usb'].get('i'),
            data['usb'].get('p'),
            data['backup'].get('v'),
            data['backup'].get('i'),
            data['backup'].get('p')
        ))
        
        conn.commit()
        conn.close()
        
        print(f"✓ Data stored at {timestamp}")
        return True
        
    except Exception as e:
        print(f"❌ Database error: {e}")
        return False

def get_latest_data():
    """Get the latest sensor reading from database"""
    try:
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT * FROM sensor_data ORDER BY id DESC LIMIT 1
        ''')
        
        row = cursor.fetchone()
        conn.close()
        
        if row:
            # Convert to dictionary
            data = {
                'id': row[0],
                'timestamp': row[1],
                'l1': row[2],
                'l2': row[3],
                'l3': row[4],
                'l4': row[5],
                'temp': row[6],
                'raining': row[7],
                'mains': {'v': row[8], 'i': row[9], 'p': row[10]},
                'charger': {'v': row[11], 'i': row[12], 'p': row[13]},
                'motor': {'v': row[14], 'i': row[15], 'p': row[16]},
                'lights': {'v': row[17], 'i': row[18], 'p': row[19]},
                'usb': {'v': row[20], 'i': row[21], 'p': row[22]},
                'backup': {'v': row[23], 'i': row[24], 'p': row[25]}
            }
            return data
        return None
        
    except Exception as e:
        print(f"❌ Error retrieving data: {e}")
        return None

# ==================== WEATHER API ====================
def fetch_weather_data():
    """Fetch weather from Open-Meteo API for Bhubaneswar"""
    try:
        # Bhubaneswar coordinates
        lat, lon = 20.2961, 85.8245
        
        url = "https://api.open-meteo.com/v1/forecast"
        params = {
            "latitude": lat,
            "longitude": lon,
            "current": "weather_code,uv_index,cloud_cover",
            "hourly": "precipitation,cloud_cover",
            "forecast_days": 1
        }
        
        response = requests.get(url, params=params, timeout=5)
        response.raise_for_status()
        data = response.json()
        
        current = data.get('current', {})
        
        weather_data = {
            'uv_index': current.get('uv_index', 0),
            'rain_forecast': current.get('precipitation', 0),
            'cloud_coverage': current.get('cloud_cover', 0),
            'timestamp': datetime.now().isoformat()
        }
        
        print(f"✓ Weather data fetched: UV={weather_data['uv_index']}, Rain={weather_data['rain_forecast']}, Clouds={weather_data['cloud_coverage']}")
        return weather_data
    
    except Exception as e:
        print(f"❌ Weather API error: {e}")
        return {
            'uv_index': 0,
            'rain_forecast': 0,
            'cloud_coverage': 0,
            'timestamp': datetime.now().isoformat()
        }

# Global weather storage
current_weather = fetch_weather_data()
last_weather_update = datetime.now()


# ==================== FLASK ROUTES ====================

@app.route('/weather', methods=['GET'])
def get_weather():
    """Get current weather data"""
    global current_weather, last_weather_update
    
    # Update every 10 minutes
    if (datetime.now() - last_weather_update).total_seconds() > 600:
        current_weather = fetch_weather_data()
        last_weather_update = datetime.now()
    
    return jsonify(current_weather), 200

@app.route('/dashboard-data', methods=['GET'])
def get_dashboard_data():
    """Get latest data formatted for dashboard"""
    global current_weather, last_weather_update
    
    # Update weather every 10 minutes
    if (datetime.now() - last_weather_update).total_seconds() > 600:
        current_weather = fetch_weather_data()
        last_weather_update = datetime.now()
    
    try:
        data = get_latest_data()
        
        if data:
            dashboard_data = {
                # Load Status (L1, L2, L3, L4)
                'l1': data['l1'],
                'l2': data['l2'],
                'l3': data['l3'],
                'l4': data['l4'],
                
                # Sensors
                'temp': data['temp'],
                'raining': data['raining'],
                
                # Solar (same as Mains)
                'mains': data['mains'],
                
                # Devices (Motor, Lights, USB, Charger, Backup)
                'motor': data['motor'],
                'lights': data['lights'],
                'usb': data['usb'],
                'charger': data['charger'],
                'backup': data['backup'],
                
                # Weather
                'weather': current_weather,
                
                # Timestamp
                'timestamp': data['timestamp']
            }
            return jsonify(dashboard_data), 200
        else:
            return jsonify({'error': 'No data available'}), 404
    
    except Exception as e:
        return jsonify({'error': str(e)}), 500
    
@app.route('/telemetry-history', methods=['GET'])
def get_telemetry_history():
    """Get sensor telemetry history (last 20 readings)"""
    try:
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT timestamp, temp, raining FROM sensor_data 
            ORDER BY id DESC LIMIT 20
        ''')
        
        rows = cursor.fetchall()
        conn.close()
        
        history = []
        for row in reversed(rows):  # Reverse to show oldest first
            history.append({
                'timestamp': row[0],
                'temp': row[1],
                'raining': row[2]
            })
        
        return jsonify(history), 200
    
    except Exception as e:
        return jsonify({'error': str(e)}), 500
    
@app.route('/sensor', methods=['POST'])
def receive_sensor_data():
    """Receive JSON payload from ESP32"""
    try:
        # Get JSON data from request
        data = request.get_json()
        
        if not data:
            return jsonify({'status': 'error', 'message': 'No data received'}), 400
        
        # Log received data
        print("\n📥 Data received from ESP32:")
        print(json.dumps(data, indent=2))
        
        # Store in database
        if store_sensor_data(data):
            response = {
                'status': 'success',
                'message': 'Data received and stored',
                'timestamp': datetime.now().isoformat()
            }
            print(f"✓ Response: {response['status']}\n")
            return jsonify(response), 200
        else:
            return jsonify({'status': 'error', 'message': 'Database storage failed'}), 500
    
    except Exception as e:
        print(f"❌ Error: {e}\n")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@app.route('/sensor', methods=['GET'])
def get_sensor_data():
    """Get latest sensor data"""
    try:
        data = get_latest_data()
        
        if data:
            return jsonify(data), 200
        else:
            return jsonify({'status': 'error', 'message': 'No data available'}), 404
    
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)}), 500

@app.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({'status': 'backend running', 'timestamp': datetime.now().isoformat()}), 200

@app.route('/history', methods=['GET'])
def get_history():
    """Get last N sensor readings (default 50)"""
    try:
        limit = request.args.get('limit', 50, type=int)
        
        conn = sqlite3.connect(DATABASE)
        cursor = conn.cursor()
        
        cursor.execute(f'''
            SELECT * FROM sensor_data ORDER BY id DESC LIMIT {limit}
        ''')
        
        rows = cursor.fetchall()
        conn.close()
        
        history = []
        for row in rows:
            history.append({
                'id': row[0],
                'timestamp': row[1],
                'l1': row[2],
                'l2': row[3],
                'l3': row[4],
                'l4': row[5],
                'temp': row[6],
                'raining': row[7],
                'mains': {'v': row[8], 'i': row[9], 'p': row[10]},
                'charger': {'v': row[11], 'i': row[12], 'p': row[13]},
                'motor': {'v': row[14], 'i': row[15], 'p': row[16]},
                'lights': {'v': row[17], 'i': row[18], 'p': row[19]},
                'usb': {'v': row[20], 'i': row[21], 'p': row[22]},
                'backup': {'v': row[23], 'i': row[24], 'p': row[25]}
            })
        
        return jsonify(history), 200
    
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)}), 500

# ==================== ERROR HANDLERS ====================

@app.errorhandler(404)
def not_found(error):
    return jsonify({'status': 'error', 'message': 'Endpoint not found'}), 404

@app.errorhandler(500)
def internal_error(error):
    return jsonify({'status': 'error', 'message': 'Internal server error'}), 500


# ==================== MAIN ====================
@app.route('/')
def serve_dashboard():
    """Serve dashboard.html"""
    return app.send_static_file('dashboard.html')

@app.before_request
def setup():
    app.static_folder = '.'  # Current directory
    app.static_url_path = ''

if __name__ == '__main__':
    print("\n" + "="*60)
    print("SIH 2026 Backend Server")
    print("="*60)
    
    # Initialize database
    init_database()
    
    # Run Flask server
    print("\n🚀 Starting Flask server...")
    print("Listening on http://0.0.0.0:8000")
    print("="*60 + "\n")
    
    app.run(host='0.0.0.0', port=8000, debug=True)
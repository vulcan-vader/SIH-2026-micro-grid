from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import sqlite3
from datetime import datetime

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


class SensorData(BaseModel):
    temperature: float
    humidity: float


# Connect to the SQLite database
connection = sqlite3.connect("sensor.db", check_same_thread=False)
cursor = connection.cursor()


# Create the readings table if it does not already exist
cursor.execute("""
CREATE TABLE IF NOT EXISTS readings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    temperature REAL NOT NULL,
    humidity REAL NOT NULL,
    timestamp TEXT NOT NULL
)
""")

connection.commit()


@app.post("/sensor")
def receive_sensor_data(data: SensorData):

    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    cursor.execute(
        """
        INSERT INTO readings (temperature, humidity, timestamp)
        VALUES (?, ?, ?)
        """,
        (data.temperature, data.humidity, timestamp)
    )

    connection.commit()

    return {
        "message": "Data saved",
        "temperature": data.temperature,
        "humidity": data.humidity,
        "timestamp": timestamp
    }


@app.get("/sensor")
def get_sensor_data():

    cursor.execute("""
        SELECT temperature, humidity, timestamp
        FROM readings
        ORDER BY id DESC
        LIMIT 1
    """)

    row = cursor.fetchone()

    if row is None:
        return {
            "temperature": 0,
            "humidity": 0,
            "timestamp": None
        }

    return {
        "temperature": row[0],
        "humidity": row[1],
        "timestamp": row[2]
    }


@app.get("/history")
def get_history():

    cursor.execute("""
        SELECT temperature, humidity, timestamp
        FROM readings
        ORDER BY id DESC
    """)

    rows = cursor.fetchall()

    history = []

    for row in rows:
        history.append({
            "temperature": row[0],
            "humidity": row[1],
            "timestamp": row[2]
        })

    return history
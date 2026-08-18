from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

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


latest_data = {
    "temperature": 0,
    "humidity": 0
}


@app.post("/sensor")
def receive_sensor_data(data: SensorData):
    global latest_data

    latest_data = {
        "temperature": data.temperature,
        "humidity": data.humidity
    }

    return {
        "message": "Data received",
        "data": latest_data
    }


@app.get("/sensor")
def get_sensor_data():
    return latest_data
"test"
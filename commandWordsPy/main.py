import websockets
import asyncio
import json
import wave
from vosk import Model, KaldiRecognizer, SetLogLevel
from websockets.exceptions import ConnectionClosedError


commands = {
    "włącz światło": 1,
    "wyłącz światło": 2,
    "zapal światło": 1,
    "zgaś światło": 2,
    "tak": 1,
    "nie": 2
}

SetLogLevel(-1)

LINK = "ws://192.168.1.35/ws" # <----- Setup here !!!!!--------------------------------
model = Model("models/vosk-model-small-pl-0.22")
rec = KaldiRecognizer(model, 16000)

async def receiveData():
    wav_file = wave.open("output.wav", 'wb')
    wav_file.setnchannels(1)
    wav_file.setsampwidth(2)
    wav_file.setframerate(16000)

    while True:
        try:
            ws = await websockets.connect(LINK)
            print("Połączono")
            while True:
                #message = await asyncio.wait_for(ws.recv(), timeout=5.0)
                message = await asyncio.wait_for(ws.recv(), timeout=None)
                wav_file.writeframes(message)
                if rec.AcceptWaveform(message):
                    words = (json.loads(rec.Result()))['text']
                    if words:
                        for command in commands:
                            if command in words:
                                print(command)
                                print(commands[command])
                                print()
                                await ws.send(bytes([commands[command]])) # only string or binary frame 
                                break
        except(asyncio.TimeoutError, OSError, ConnectionClosedError):
            for i in range(5, 0, -1):
                print(f"Ponowna próba połączenia za {i} sek.")
                await asyncio.sleep(1)
            

if __name__ == "__main__":
    try:
        asyncio.run(receiveData())
    except(KeyboardInterrupt):
        print("\nProgram przerwany: [Ctrl+C]")
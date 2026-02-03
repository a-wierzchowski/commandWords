import websockets
import asyncio
import json
from vosk import Model, KaldiRecognizer, SetLogLevel


commands = {
    "włącz światło": 1,
    "wyłącz światło": 2,
    "zapal światło": 1,
    "zgaś światło": 2
}

SetLogLevel(-1)

LINK = "ws://192.168.1.35/ws" # <----- Setup here !!!!!--------------------------------
model = Model("models/vosk-model-small-pl-0.22")
rec = KaldiRecognizer(model, 16000)

async def receiveData():
    while True:
        try:
            ws = await websockets.connect(LINK, ping_interval=None)
            print("Połączono")
            while True:
                message = await asyncio.wait_for(ws.recv(), timeout=5.0)
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
        except(asyncio.TimeoutError, OSError):
            for i in range(5, 0, -1):
                print(f"Ponowna próba połączenia za {i} sek.")
                await asyncio.sleep(1)
            

if __name__ == "__main__":
    try:
        asyncio.run(receiveData())
    except(KeyboardInterrupt):
        print("\nProgram przerwany: [Ctrl+C]")
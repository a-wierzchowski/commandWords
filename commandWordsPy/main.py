import websockets
import asyncio
import time
from vosk import Model, KaldiRecognizer


LINK = "ws://192.168.1.35/ws" # <----- Setup here !!!!!--------------------------------
model = Model("models/vosk-model-small-pl-0.22")
rec = KaldiRecognizer(model, 16000)

async def receiveData():
    while True:
        try:
            ws = await websockets.connect(LINK, ping_interval=None)
            while True:
                message = await ws.recv()
                if rec.AcceptWaveform(message):
                    print('\n' + rec.Result())
        except:
            for i in range(5, 0, -1):
                print(f"Ponowna próba połączenia za {i} sek.")
                await asyncio.sleep(1)
            

if __name__ == "__main__":
    asyncio.run(receiveData())
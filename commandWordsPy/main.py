import websockets
import asyncio


LINK = "ws://192.168.1.35/ws" # <----- Setup here !!!!!--------------------------------


async def receiveData():
    while True:
        try:
            ws = await websockets.connect(LINK)
            while True:
                message = await ws.recv()
                print(f"Wiadomosc: {message}")
        except:
            for i in range(5, 0, -1):
                print(f"Ponowna próba połączenia za {i} sek.")
                await asyncio.sleep(1)
            print("###############\n\n\n")

if __name__ == "__main__":
    asyncio.run(receiveData())
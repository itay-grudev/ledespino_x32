#!/usr/bin/env python3

import pathlib
import asyncio
import websockets

async def new_socket(websocket, path):
    setup = open( pathlib.Path(__file__).parent / 'setup', 'r' ).read()
    await websocket.send( setup )

    while True:
        await asyncio.sleep( 2 )
        status = open( pathlib.Path(__file__).parent / 'status', 'r' ).read()
        await websocket.send( status )

start_server = websockets.serve( new_socket, '0.0.0.0', 8765 )

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()

#!/usr/bin/env python3

import aiohttp
import asyncio
import os

from aiohttp import web

# For more server examples see:
# https://aiohttp.readthedocs.io/en/stable/web_quickstart.html

WWW_DIR = os.path.join( os.path.dirname(__file__), '../main/html/' )
JSON_DIR = os.path.dirname(__file__)

async def index(request):
    index = open( os.path.join( WWW_DIR, 'index.html'), 'r' ).read()
    return web.Response(
        body = index,
        headers = {
            'Content-Type': 'text/html'
        }
    )

async def websocket_handler( request ):
    ws = web.WebSocketResponse()
    await ws.prepare( request )

    setup = open( os.path.join( JSON_DIR, 'setup.json'), 'r' ).read()
    await ws.send_str( setup )

    while True:
        await asyncio.sleep( 2 )
        status = open( os.path.join( JSON_DIR, 'status.json'), 'r' ).read()
        await ws.send_str( status )
    # async for msg in ws:
    #     if msg.type == aiohttp.WSMsgType.TEXT:
    #         if msg.data == 'close':
    #             await ws.close()
    #         else:
    #             await ws.send_str(msg.data + '/answer')
    #     elif msg.type == aiohttp.WSMsgType.ERROR:
    #         print('ws connection closed with exception %s' %
    #               ws.exception())

    return ws

app = aiohttp.web.Application()
app.router.add_get( '/', index )
app.add_routes([ web.get( '/ws', websocket_handler )])
web.run_app( app, host = '0.0.0.0', port = 8000 )

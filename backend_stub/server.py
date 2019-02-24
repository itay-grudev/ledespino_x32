#!/usr/bin/env python3

import aiohttp
import asyncio
import json
import os

from aiohttp import web

# For more server examples see:
# https://aiohttp.readthedocs.io/en/stable/web_quickstart.html

WWW_DIR = os.path.join( os.path.dirname(__file__), '../main/html/' )
JSON_DIR = os.path.dirname(__file__)

device_json = open( os.path.join( JSON_DIR, 'setup.json'), 'r' ).read()
status = open( os.path.join( JSON_DIR, 'status.json'), 'r' ).read()

async def index(request):
    index_body = open( os.path.join( WWW_DIR, 'index.html'), 'r' ).read()
    return web.Response(
        body = index_body,
        headers = {
            'Content-Type': 'text/html'
        }
    )

sockets = []
state = json.loads( device_json )

async def websocket_handler( request ):
    ws = web.WebSocketResponse()
    await ws.prepare( request )
    sockets.append( ws )

    await ws.send_str( json.dumps( state ) )

    async for msg in ws:
        if msg.type == aiohttp.WSMsgType.TEXT:
            state_changes = json.loads( msg.data )
            for key in state_changes:
                if isinstance( state_changes[key], dict ):
                    index = state_changes[key]["i"]
                    for _, item in enumerate( state[key] ):
                        if item["i"] == index:
                            for subkey in state_changes[key]:
                                item[subkey] = state_changes[key][subkey]
                else:
                    state[key] = state_changes[key]
            for socket in sockets:
                if socket == ws:
                    continue
                try:
                    await socket.send_str( msg.data )
                except Exception as e:
                    logger.error( 'Failed to send message ' + str(e) )
        elif msg.type == aiohttp.WSMsgType.ERROR:
            sockets.remove( ws )
            print( 'ws connection closed with exception %s' % ws.exception() )

    return ws

app = aiohttp.web.Application()
app.router.add_get( '/', index )
app.add_routes([ web.get( '/ws', websocket_handler )])
web.run_app( app, host = '0.0.0.0', port = 8000 )

from mod_pywebsocket import handshake
from mod_pywebsocket.handshake.hybi import compute_accept


def web_socket_do_extra_handshake(request):
    msg = 'HTTP/1.1 101 Switching Protocols\n'  # Does not end with "\r\n".
    msg += 'Upgrade: websocket\r\n'
    msg += 'Connection: Upgrade\r\n'
    msg += 'Sec-WebSocket-Accept: %s\r\n' % compute_accept(request.headers_in['Sec-WebSocket-Key'])[0]
    msg += '\r\n'
    request.connection.write(msg)
    print msg
    # Prevents pywebsocket from sending its own handshake message.
    raise handshake.AbortedByUserException('Abort the connection')


def web_socket_transfer_data(request):
    pass

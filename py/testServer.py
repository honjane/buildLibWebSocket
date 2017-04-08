#-*- coding:utf8 -*-

import threading
import hashlib
import socket
import base64

class websocket_thread(threading.Thread):
    def __init__(self, connection):
        super(websocket_thread, self).__init__()
        self.connection = connection

    def run(self):
        print 'new websocket client joined!'
        #服务器回执消息
        reply = 'i got u, from websocket server.'

        length = len(reply)
        while True:
            data = self.connection.recv(1024)
            print parse_data(data)
            self.connection.send('%c%c%s' % (0x81, length, reply))

def parse_data(msg):
    v = ord(msg[1]) & 0x7f
    if v == 0x7e:
        p = 4
    elif v == 0x7f:
        p = 10
    else:
        p = 2
    mask = msg[p:p+4]
    data = msg[p+4:]

    return ''.join([chr(ord(v) ^ ord(mask[k%4])) for k, v in enumerate(data)])

def parse_headers(msg):
    headers = {}
    header, data = msg.split('\r\n\r\n', 1)
    for line in header.split('\r\n')[1:]:
        key, value = line.split(': ', 1)
        headers[key] = value
    headers['data'] = data
    return headers

def generate_token(msg):
    key = msg + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11'
    ser_key = hashlib.sha1(key).digest()
    return base64.b64encode(ser_key)

if __name__ == '__main__':
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    #修改为服务器地址及端口号
    sock.bind(('172.31.78.51', 9000))

    sock.listen(5)
    print 'start success'
    while True:
        connection, address = sock.accept()
        try:
            data = connection.recv(1024)
            headers = parse_headers(data)
            token = generate_token(headers['Sec-WebSocket-Key'])
            connection.send('\
HTTP/1.1 101 WebSocket Protocol Hybi-10\r\n\
Upgrade: WebSocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\r\n' % token)
            thread = websocket_thread(connection)
            thread.start()
        except socket.timeout:
            print 'websocket connection timeout'
    print '1'

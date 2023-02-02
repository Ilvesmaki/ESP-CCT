#!/usr/bin/env python
import http.server
import http.client
import os
import paho.mqtt.client as mqtt_client
import json
import threading
import argparse

WEB_SERVER_HOSTNAME = "localhost"
WEB_SERVER_PORT = 8080 #You can choose any available port; by default, it is 8000
dataNames = {"ww_cw_ratio": "0.5", "output": "OFF", "brightness": "0.5"}

MQ_CLIENT_NAME = "python_web_server"
MQ_SUB_TOPIC_NAME = "KITCHEN/OUT/LIGHTS"
MQ_PUB_TOPIC_NAME = "KITCHEN/IN/LIGHTS"

sub_topic = ""
pub_topic = ""

argParser = argparse.ArgumentParser()
argParser.add_argument("-n", "--name", type=str, default="test", help="Mqtt server username")
argParser.add_argument("-pa", "--password", type=str, default="test", help="Mqtt server password")
argParser.add_argument("-s", "--server", type=str, default="localhost", help="Mqtt server address")
argParser.add_argument("-po", "--port", type=int, default=1883, help="Mqtt server port")
argParser.add_argument("-sub", "--subscribe", type=str, default=MQ_SUB_TOPIC_NAME, help="Mqtt subscribe topic")
argParser.add_argument("-pub", "--publish", type=str, default=MQ_PUB_TOPIC_NAME, help="Mqtt publish topic")


client = mqtt_client.Client(client_id=MQ_CLIENT_NAME)

def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(sub_topic)

def on_message(client, userdata, msg):
    print("Message received!\nTOPIC: {}\nPAYLOAD: {}".format(msg.topic, msg.payload))
    json_object = json.loads(msg.payload)
    for key in dataNames.keys():
        if key in json_object:
            dataNames[key] = str(json_object[key])



class MyServer(http.server.BaseHTTPRequestHandler):  
    def _set_headers(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()

    def log_message(self, format, *args):
        return
        
    def do_HEAD(self):
        self._set_headers()

    def do_GET(self): # the do_GET method is inherited from BaseHTTPRequestHandler
        # print("do_GET(): client requested: {} from path: {}".format(self.requestline, self.path))
        key = self.path.strip("/")
        if(self.path == "/"):
            self._set_headers()
            file = open("index.html", "r")
            self.wfile.write(file.read().encode("utf-8"))
            file.close()
            return
        
        if(key in dataNames.keys()):
            self._set_headers()
            self.wfile.write(str(dataNames[key]).encode("utf-8"))
        else:
            # print(os.path.basename(self.path))
            file = open(os.path.basename(self.path), "rb")
            if(file.readable()):
                self.send_response(200)
                data = file.read().encode("utf-8")
                self.wfile.write(data)
                file.close()
            else:
                self.send_response(404)
        

    def do_POST(self):
        # print("do_POST(): client requested: {} from path: {}".format(self.requestline, self.path))
        key = self.path.strip("/")
        if(key not in dataNames.keys()):
            self.send_response(404)
            # print("requested value {} was not found!".format(self.path))
            return
        self._set_headers()

        data_string = self.rfile.read(int(self.headers['Content-Length']))

        self.send_response(200)
        self.end_headers()

        value_str = data_string.decode("utf-8")
        dataNames[key] = value_str
        if(key == "output"):
            value = '"' + value_str + '"'
        else:
            value = float(value_str)
        json_string = '{{"{}": {}}}'.format(key, value)
        # print(json_string)
        client.publish(pub_topic, json_string)

if __name__ == "__main__":

    args = argParser.parse_args()

    username = args.name
    password = args.password
    server = args.server
    port = args.port
    pub_topic = args.publish
    sub_topic = args.subscribe

    client.on_connect = on_connect
    client.on_message = on_message
    client.username_pw_set(username, password)
    client.connect(server, port, 60)
    mq_thread = threading.Thread(target=client.loop_forever)
    mq_thread.start()
    
    webServer = http.server.HTTPServer((WEB_SERVER_HOSTNAME, WEB_SERVER_PORT), MyServer)
    print("Server started http://%s:%s" % (WEB_SERVER_HOSTNAME, WEB_SERVER_PORT))  #Server starts
    try:
        webServer.serve_forever()
    except KeyboardInterrupt:
        pass
    webServer.server_close()  #Executes when you hit a keyboard interrupt, closing the server
    client.disconnect()
    mq_thread.join()
    print("Server stopped.")
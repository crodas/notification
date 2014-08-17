Notification Push Server
========================

The idea behind this project is to create an easy way of delivering notifications/messages to peers in real time over HTTP.

Supports long polling so far but with plans to support WebSockets.

Basic Configuration
-------------------

It is an stand alone server, it expects a `json` file with the basic configuration.

```json
{
    "http_port": 8080,
    "http_ip": "0.0.0.0",
    "allow_origin": "*.foobar.com",
    "debug": true,
    "allow_post": false, 
    "daemon_ip": "127.0.0.1",
    "daemon_port": 2508
}
```

 * http_port: port to listen to.
 * http_ip: ip to bind to
 * allow_origin: Set `Access-Control-Allow-Origin` origin, to allow safely cross domain ajax calls
 * debug: Enable debug logging to the `stdout` and output a nicer-format `json`
 * allow_post: Accept messages creation via POST. Otherwise messages needs to be generated via backend
 * daemon_ip: The deamon (should be private) the daemon should be binded to.
 * daemon_port: To what port should the daemon be binded to.
 
Instalation
-----------

[Nanomsg](http://nanomsg.org/download.html) and `CMake` are required in order to build from source.

```bash
cmake .
make
./notifd ./config.json
```

Url
---

Connection subscribe to `channels`.

```bash
curl -v http://localhost:8080/channel/<id>
```

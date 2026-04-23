# This project has been created as part of the 42 curriculum by Hguiller and Molapoug 

# Description

## Webserv

Webserv is a custom, lightweight HTTP server written from scratch in C++. Inspired by Nginx, it is built around a non-blocking, event-driven architecture using epoll to handle multiple client connections and requests concurrently and efficiently.
Key Features:

    Event-Driven I/O: Fully non-blocking socket multiplexing using epoll for high-performance network communication.

    Custom Configuration: Parses an Nginx-style .conf file to set up multiple virtual servers with distinct ports, hostnames, and routing rules (Locations).

    HTTP Protocol: Supports standard HTTP methods (GET, POST, and DELETE), chunked requests, and custom error pages.

    Dynamic CGI Execution: Seamlessly handles Common Gateway Interface (CGI) scripts (e.g., Python, PHP) with dedicated process management, non-blocking pipes, and built-in timeout protection.

    Static File & Directory Management: Efficiently serves static files, handles file uploads, and features automatic directory listing (autoindex).

# Instructions

## Compile

```bash
make
```

## Run Server

```bash
./Webserv configure.conf
```

## Link of the Server

to test the front : [http://localhost:8086](http://localhost:8086)

## Comand Curl

```bash
# Command GET to get home page
curl -v -X GET http://localhost:8086/

# POST Method to create a file contain "I hate tasty crousty"
curl -X POST -d "<h1>hello</h1>" http://localhost:8086/test.html

# GET Method to print the output
curl -v -X GET http://localhost:8086/test.html

# DELETE Methode to delete a file
curl -v -X DELETE http://localhost:8086/test.html

# Upload file
curl -v -X POST -F "file=@<ton_fichier>" http://localhost:8086/upload_file_script

```

## 📚 Resources

Here is a curated list of the essential resources and documentation that helped build this HTTP server from scratch.

### 🌐 HTTP Protocol
* **[RFC 9110 (Semantics) & RFC 9112 (HTTP/1.1)](https://httpwg.org/specs/) :** The official specifications (which replace the older RFC 2616). Absolute must-reads for understanding request/response formatting, headers, and status codes.
* **[MDN Web Docs - HTTP](https://developer.mozilla.org/en-US/docs/Web/HTTP) :** Mozilla's documentation provides a clearer, more digestible explanation of HTTP methods (GET, POST, DELETE), headers, and MIME types.

### 🔌 Sockets & Network Programming
* **[Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) :** The ultimate bible for network programming in C/C++. It thoroughly explains how to create sockets, bind ports, listen for connections, and accept clients.

### ⚙️ Multiplexing & Non-Blocking I/O
* **`man epoll(7)` :** The core of our server's architecture. The official Linux manual pages for `epoll_create`, `epoll_ctl`, and `epoll_wait` are essential for understanding how to handle multiple concurrent client connections efficiently.
* **`man fcntl(2)` :** Crucial for understanding how to set sockets and file descriptors to non-blocking mode (`O_NONBLOCK`).

### 🐍 CGI (Common Gateway Interface)
* **[RFC 3875 - CGI Version 1.1](https://datatracker.ietf.org/doc/html/rfc3875) :** The standard detailing how a web server should communicate with external scripts (like Python or PHP). It defines the required environment variables (`CONTENT_LENGTH`, `PATH_INFO`, etc.) and standard I/O behavior.

### 📝 Configuration File
* **[Nginx Documentation](https://nginx.org/en/docs/) :** Since our custom `.conf` syntax is heavily inspired by Nginx, their documentation serves as a great reference for structuring `server` and `location` blocks, as well as routing logic.

### 🤖 AI Usage
* Gemini
* Copilot
* ChatGPT
* write faster code
* debugging
* understand how epoll works
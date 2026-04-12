# Webserv

> Un serveur HTTP minimaliste en C++98 avec support du multiplexing asynchrone, du virtual hosting, des CGI et d'un parsing de configuration.

---

## Table des matières

- [Description](#description)
- [Architecture](#architecture)
- [Fonctionnalités](#fonctionnalités)
- [Installation](#installation)
- [Utilisation](#utilisation)
- [Structure du projet](#structure-du-projet)
- [Concepts clés](#concepts-clés)

---

## Description

**Webserv** est un serveur web HTTP/1.1 écrit en **C++98** qui implémente les fonctionnalités essentielles d'un serveur web production-ready :

- **Multiplexing asynchrone** via `epoll` pour gérer plusieurs clients simultanément
- **Virtual Hosting** pour servir plusieurs domaines sur une même instance
- **Support des CGI** (PHP, Python, Perl) pour l'exécution de scripts dynamiques
- **Parsing de configuration** flexible inspiré de Nginx
- **Gestion complète des méthodes HTTP** (GET, POST, DELETE)
- **Auto-indexing** et pages d'erreur personnalisées

---

## Architecture

### Vue d'ensemble du flux client

```
┌─────────────────────────────────────────────────────────────┐
│                    CLIENT HTTP                              │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
         ┌───────────────────────────┐
         │   NEW CONNECTION          │
         │   accept() → addToEpoll() │
         └────────┬──────────────────┘
                  │
        ┌─────────┴─────────┐
        │                   │
        ▼                   ▼
   ┌─────────┐         ┌──────────┐
   │ EPOLLIN │         │ EPOLLOUT │
   │ (Lire)  │         │ (Écrire) │
   └────┬────┘         └────┬─────┘
        │                   │
        ▼                   ▼
    recv()              send()
    buffer              buffer
    complet?            complet?
        │                   │
        ├─── OUI ───►HTTP   ├─── OUI ──► closeClient()
        │       Response    │
        │                   ├─── NON ──► retry on next EPOLLOUT
        └─── NON ───► wait  │
                next EPOLLIN └─ ERROR ──► closeClient()
```

### Cycle de vie du multiplexing asynchrone

```
┌──────────────────────────────────────────────────────────────┐
│                    EVENT LOOP (epoll)                        │
└──────────────────────────────────────────────────────────────┘

1. Nouvelle connexion (port d'écoute)
   └─► accept() + fcntl(O_NONBLOCK) + addToEpoll(EPOLLIN)

2. Client envoie requête
   └─► EPOLLIN triggered
       └─► recv() ─► accumuler dans _client_buffers
           └─► Requête complète?
               ├─ OUI  ─► parse request
               │         └─► construire response dans _write_buffers[fd]
               │         └─► setEpollOut() [EPOLLIN → EPOLLOUT]
               │
               └─ NON  ─► continue accumulating (wait next EPOLLIN)

3. Buffer d'écriture prêt
   └─► EPOLLOUT triggered
       └─► send() depuis _write_buffers[fd]
           └─► Tout envoyé?
               ├─ OUI  ─► closeClient()
               ├─ NON  ─► erase() ce qui est parti
               │         └─► attendre prochain EPOLLOUT
               └─ ERROR ─► closeClient()
```

### Sélection de la configuration (Virtual Hosting)

```
┌──────────────────────────────┐
│   Requête HTTP parsée        │
│   Header "Host": "api.com"   │
│   Port: 8080                 │
└──────────────┬───────────────┘
               │
               ▼
    ┌─────────────────────────┐
    │ Chercher correspondance │
    └──────────┬──────────────┘
               │
    ┌──────────▼──────────┐
    │ Port == 8080?       │
    │ server_name == Host?│
    └──────┬───────────┬──┘
           │ OUI      │ NON
           │          │
           ▼          ▼
      ┌─────────┐  ┌──────────────┐
      │ Utiliser│  │ Utiliser la  │
      │ config  │  │ ServerConfig │
      │ exacte  │  │ par défaut   │
      └─────────┘  │ (1ere pour   │
                   │  ce port)    │
                   └──────────────┘
```

### Pipeline de traitement des requêtes HTTP

```
┌──────────────────────────────────┐
│        HTTP Request              │
│  GET /api/file.txt HTTP/1.1      │
└────────────┬─────────────────────┘
             │
             ▼
    ┌────────────────────┐
    │  resolveFilePath() │ (LocationConfig matching)
    └────────┬───────────┘
             │
    ┌────────▼────────┐
    │ Method allowed? │
    └────┬──────┬─────┘
    YES  │      │  NO
         │      └─► HTTP 405 (Method Not Allowed)
         │
         ▼
    ┌─────────────────┐
    │  req.method?    │
    └┬────────┬────┬──┘
    │        │    │
  GET      POST DELETE
    │        │    │
    ▼        ▼    ▼
  [GET]   [POST] [DELETE]
    │        │    │
    └────────┴────┴──────── HTTP Response
```

### Traitement GET

```
┌──────────────────────────────────┐
│    GET /path/to/resource        │
└────────────┬─────────────────────┘
             │
    ┌────────▼──────────┐
    │ Custom Route?     │
    │ (ex: "/api/home") │
    └────┬──────────┬───┘
    YES  │          │ NO
         │          │
         ▼          ▼
    Execute   ┌────────────┐
    Route     │ Directory? │
    Handler   └────┬───┬───┘
                YES │   │ NO
                   │   │
                   ▼   ▼
            ┌─────────────────┐
            │ Chercher index  │  ┌────────┐
            └────┬──────┬─────┘  │ File?  │
            YES  │      │ NO     └──┬──┬──┘
                 │      │       YES │  │ NO
                 ▼      ▼          ▼  ▼
              Lire   ┌──────┐   Lire  404
              Fichier│ Auto │ Fichier
               200 OK│Index?└──────┬───┘
                     └──┬──────┬───┘
                    YES │      │ NO
                        ▼      ▼
                      HTML    403
                      200 OK  Forbidden
```

### Exécution des CGI (Common Gateway Interface)

```
┌────────────────────────────────────────┐
│   Requête vers script CGI              │
│   GET /cgi-bin/script.php?id=42       │
└────────────┬───────────────────────────┘
             │
             ▼
    ┌────────────────────┐
    │ Extension CGI?     │
    │ (.php, .py, .pl)   │
    └────┬──────────┬────┘
    YES  │          │ NO
         │          └─► Servir comme fichier statique
         │
         ▼
    ┌─────────────────────┐
    │  CgiHandler init    │ buildEnv()
    │  pipe() x2 créés    │ Env: REQUEST_METHOD,
    │  (stdin + stdout)   │      QUERY_STRING, etc.
    └────────┬────────────┘
             │
             ▼
    ┌────────────────────┐
    │   fork()           │
    └────┬───────────┬───┘
         │ child     │ parent
         │ (0)       │ (>0)
         │           │
         ▼           ▼
    ┌─────────┐  ┌─────────────┐
    │ dup2()  │  │ write()     │
    │ execve()│  │ stdin_pipe  │
    │ Script  │  │ ↓           │
    │ runs    │  │ waitpid()   │
    │         │  │ read()      │
    │         │  │ stdout_pipe │
    │         │  └──────┬──────┘
    └─────────┘         │
                        ▼
                   ┌──────────────┐
                   │ parseCgiOut()│
                   │ Headers+Body │
                   └──────┬───────┘
                          │
                          ▼
                   ┌──────────────┐
                   │ HTTP/1.1 200 │
                   │ Response     │
                   └──────────────┘
```

### Parsing de configuration

```
┌──────────────────────────────┐
│    config.conf (texte brut)  │
│  server {                    │
│    listen 8080;              │
│    server_name example.com;  │
│    ...                       │
│  }                           │
└────────────┬─────────────────┘
             │
             ▼
    ┌──────────────────┐
    │  tokenize()      │
    │  Découpe en      │
    │  tokens bruts    │
    └──────┬───────────┘
           │
           ▼
    ["server", "{", "listen", "8080", ";", "}"]
           │
           ▼
    ┌──────────────────┐
    │  parse()         │
    │  Descending      │
    │  Parser          │
    │  peek()/get()    │
    └──────┬───────────┘
           │
           ▼
    ┌──────────────────────┐
    │  ServerConfig        │
    │  LocationConfig      │
    │  Structures C++      │
    └──────┬───────────────┘
           │
           ▼
    Read-only durant
    toute la vie du serveur
```

---

## Fonctionnalités

### Cœur du serveur

| Fonctionnalité | Implémentation |
|---|---|
| **Multiplexing asynchrone** | `epoll` (Linux) avec `fcntl(O_NONBLOCK)` |
| **Virtual Hosting** | Routage par `Host` header + port d'écoute |
| **Configuration dynamique** | Parser custom (style Nginx) |
| **Gestion des clients** | Maps de buffers (read/write) par file descriptor |

### Méthodes HTTP

| Méthode | Fonctionnalité |
|---|---|
| **GET** | Lecture fichiers statiques, index dynamique, custom routes |
| **POST** | Upload de fichiers binaires |
| **DELETE** | Suppression de fichiers |

### Pages et gestion des erreurs

- Pages d'erreur personnalisées (configurable par code d'erreur)
- Auto-indexing des répertoires (HTML généré dynamiquement)
- Status codes HTTP standards (200, 201, 204, 301, 400-599)

### CGI et Scripts

- Support PHP, Python, Perl (via `execve` + pipes)
- Variables d'environnement CGI standards
- I/O en streaming (grande taille de requête/réponse)

---

## Installation

### Prérequis

- `C++98` compiler (g++, clang++)
- Linux avec support `epoll`
- Make

### Compilation

```bash
cd Webserv
make              # Compilation
make clean        # Nettoyer les .o
make fclean       # Nettoyer complètement
make re           # Rebuild
```

Exécutable généré : `./Webserv`

---

## Utilisation

### Configuration de base

Créer un fichier `server.conf` :

```nginx
server {
    listen       8080;
    server_name  localhost;
    
    client_max_body_size 1M;
    
    location / {
        root         ./www;
        index        index.html;
        allow_methods GET POST;
        autoindex    on;
    }
    
    location /api {
        root         ./api;
        allow_methods GET;
        autoindex    off;
    }
    
    error_page 404 /404.html;
}
```

### Lancer le serveur

```bash
./Webserv server.conf
```

Le serveur écoute sur `localhost:8080` et attend les requêtes HTTP.

### Exemples de requêtes

```bash
# GET simple
curl http://localhost:8080/index.html

# GET avec index auto
curl http://localhost:8080/

# POST (upload fichier)
curl -X POST --data-binary @file.bin http://localhost:8080/upload/

# DELETE
curl -X DELETE http://localhost:8080/file.txt

# CGI PHP
curl http://localhost:8080/cgi-bin/script.php?id=42
```

---

## Structure du projet

```
Webserv/
├── Makefile                 # Build configuration
├── README.md               # This file
│
├── incs/                   # Headers (.hpp)
│   ├── Webserv.hpp         # Main includes
│   ├── ServerConfig.hpp    # Server configuration
│   ├── LocationConfig.hpp  # Location block
│   ├── ServerManager.hpp   # Event loop manager
│   ├── HttpRequest.hpp     # Request parser
│   ├── HttpRespons.hpp     # Response generator
│   ├── Parser.hpp          # Config parser
│   ├── Routes.hpp          # Custom route handlers
│   ├── CgiHandler.hpp      # CGI execution
│   └── colors.hpp          # Terminal colors
│
├── srcs/                   # Sources (.cpp)
│   ├── main.cpp            # Entry point
│   ├── ServerManager.cpp   # Event loop
│   ├── ServerConfig.cpp    # Server config
│   ├── LocationConfig.cpp  # Location config
│   ├── HttpRequest.cpp     # Request parsing
│   ├── HttpRespons.cpp     # Response building
│   ├── Parser.cpp          # Config parsing
│   ├── Routes.cpp          # Route handling
│   └── CgiHandler.cpp      # CGI execution
│
├── cgi-bin/                # CGI scripts (PHP, Python, etc.)
├── www/                    # Web root (static files)
├── routes/                 # Custom route handlers
├── scripts/                # Utility scripts
├── test.conf              # Example configuration
└── .gitignore             # Git exclusions
```

---

## Concepts clés

### 1. **Multiplexing avec epoll**

Le serveur n'utilise **pas** de threads. Au lieu de cela, il utilise `epoll` (efficient polling) pour gérer plusieurs clients simultanément :

- Chaque socket client est mis en mode **non-bloquant** (`O_NONBLOCK`)
- `epoll_wait()` bloque jusqu'à ce qu'un événement survienne
- Les événements sont traités en boucle : EPOLLIN (données à lire), EPOLLOUT (prêt à écrire)

**Avantage** : haute concurrence avec minimum de ressources.

### 2. **Buffers d'accumulation**

Les requêtes HTTP peuvent arriver fragmentées sur le réseau. Le serveur utilise deux maps :

```cpp
std::map<int, std::string> _client_buffers;   // Accumulation requête
std::map<int, std::string> _write_buffers;    // Réponse à envoyer
```

La requête est parsée seulement quand elle est **complète** (détection `\r\n\r\n`).

### 3. **Virtual Hosting**

Un seul serveur Webserv peut héberger **plusieurs domaines** :

```nginx
server {
    listen 8080;
    server_name api.example.com;
    ...
}

server {
    listen 8080;
    server_name web.example.com;
    ...
}
```

La sélection se fait via l'en-tête HTTP `Host` + le port d'écoute.

### 4. **Parsing de configuration**

Le Parser fonctionne en **deux passes** :

1. **tokenize()** : découpe le fichier texte en tokens primitifs
2. **parse()** : descending parser qui construit les structures C++

Exemple :
```
Fichier texte → ["server", "{", "listen", "8080", ";", "}"]
                              ↓
           ServerConfig + LocationConfig (structures C++)
```

### 5. **Exécution CGI**

Pour exécuter un script PHP/Python/Perl :

1. **fork()** : créer un processus enfant
2. **dup2()** : rediriger stdin/stdout vers des pipes
3. **execve()** : remplacer le processus enfant par l'interpréteur
4. **waitpid()** : parent attend la fin du script
5. **Lecture du stdout** : capturer la réponse

C'est le pattern classique Unix `fork → execve`.

### 6. **Gestion des erreurs HTTP**

Le serveur gère les erreurs avec des codes HTTP standards :

| Code | Signification |
|---|---|
| **200** | OK |
| **201** | Created (POST success) |
| **204** | No Content |
| **301** | Moved Permanently |
| **400** | Bad Request |
| **403** | Forbidden (autoindex off) |
| **404** | Not Found |
| **405** | Method Not Allowed |
| **500** | Internal Server Error |
| **505** | HTTP Version Not Supported |

---

## Notes

- Code en **C++98** (compatibilité maximale)
- Pas de dépendances externes
- Compilé avec `-Wall -Werror -Wextra`
- Testé sur Linux (epoll support required)
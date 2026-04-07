# Gestion serveur/client

nouvelle connexion

    → handelNewConnection() → accept() → addToEpoll(EPOLLIN)

epoll EPOLLIN sur client

    → handleClient() → recv() → accumuler dans _client_buffers
    → requête complète ? → construire réponse → _write_buffers[fd]
    → setEpollOut() → EPOLL_CTL_MOD vers EPOLLOUT

epoll EPOLLOUT sur client

    → sendResponse() → send() depuis _write_buffers
    → tout envoyé ? → closeClient()
    → partiel ? → erase() ce qui est parti → on reviendra au prochain EPOLLOUT

closeClient()

    → EPOLL_CTL_DEL
    → erase() dans les 3 maps
    → close(fd)

# Gestion du multiplexing

    accept() → fcntl(O_NONBLOCK) → addToEpoll(EPOLLIN)

    EPOLLIN → recv()
        EAGAIN → return (epoll rappellera)
        0      → closeClient (déconnexion propre)
        >0     → accumuler → requête complète ? → _write_buffers → setEpollOut()

    EPOLLOUT → send()
        EAGAIN → return (buffer kernel plein, epoll rappellera)
        <0     → closeClient
        ok     → erase() → tout envoyé ? → closeClient

___
# Virtual Hosting (Routage par server_name)

Sélection de la bonne configuration pour un client

    → Extraire l'en-tête "Host" depuis la requête parsée (ex: "tasty:8080" ou "localhost")
    → Identifier le port d'arrivée du client via _client_to_config
    → Parcourir _configs → correspondance exacte trouvée (port == port d'arrivée ET server_name == Host) ?
        oui → utiliser cette ServerConfig spécifique
        non → utiliser la ServerConfig par défaut (la première déclarée pour ce port)

___
# Traitement des Méthodes HTTP (HttpResponse)

Validation de la route (resolveFilePath)

    → Chercher la LocationConfig qui matche le plus longuement le chemin de la requête
    → Vérifier si req.method est présente dans allow_methods
        non → throw 405 (Method Not Allowed)
        oui → concaténer root + chemin de la requête

Logique GET

    → path est un dossier ? 
        → chercher le fichier index → trouvé ? lire le fichier
        → pas d'index ? vérifier autoindex → on ? générer page HTML (generateAutoindex) → off ? throw 403
    → path est un fichier ? 
        → fileExists() ? lire et renvoyer 200 OK → sinon throw 404

Logique POST (Upload)

    → path est un dossier ? → throw 403
    → std::ofstream en mode écriture (out | binary)
    → écrire le contenu complet de req.body dans le fichier cible
    → succès ? renvoyer 201 Created

Logique DELETE

    → fileExists() ?
        non → throw 404
        oui → std::remove() du fichier → succès ? renvoyer 200 OK → échec ? throw 403

___
# Parsing

	Le Parser transforme le fichier texte en structures de données C++. Il fonctionne en deux passes : d'abord tokenize() qui découpe le texte brut en tokens (server, {, listen, 8080, ;, etc.), puis parse() qui consomme ces tokens pour construire des ServerConfig et LocationConfig.
	La tokenisation traite trois cas : les espaces/newlines séparent les tokens, les caractères {, }, ; sont eux-mêmes des tokens, et # démarre un commentaire jusqu'à la fin de ligne. Le peek()/get() avec un pos entier est un pattern classique de parser récursif descendant — tu regardes sans avancer, ou tu avances.
	Les structures de config
	ServerConfig représente un bloc server { } : un port, un nom, une taille max de body, des pages d'erreur, et une liste de LocationConfig. LocationConfig représente un bloc location /path { } : le chemin, la racine filesystem, le fichier index, si autoindex est actif, et les méthodes autorisées. Ces structures sont remplies une fois au démarrage, puis lues en lecture seule pendant toute la vie du serveur.
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

___
# Parsing

	Le Parser transforme le fichier texte en structures de données C++. Il fonctionne en deux passes : d'abord tokenize() qui découpe le texte brut en tokens (server, {, listen, 8080, ;, etc.), puis parse() qui consomme ces tokens pour construire des ServerConfig et LocationConfig.
	La tokenisation traite trois cas : les espaces/newlines séparent les tokens, les caractères {, }, ; sont eux-mêmes des tokens, et # démarre un commentaire jusqu'à la fin de ligne. Le peek()/get() avec un pos entier est un pattern classique de parser récursif descendant — tu regardes sans avancer, ou tu avances.
	Les structures de config
	ServerConfig représente un bloc server { } : un port, un nom, une taille max de body, des pages d'erreur, et une liste de LocationConfig. LocationConfig représente un bloc location /path { } : le chemin, la racine filesystem, le fichier index, si autoindex est actif, et les méthodes autorisées. Ces structures sont remplies une fois au démarrage, puis lues en lecture seule pendant toute la vie du serveur.
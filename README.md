# Popsauce
Le Popsauce est un jeu de culture générale dont le but est de trouver le plus rapidement possible l'origine d'une image ou d'un texte, d'une citation, voir juste de répondre à une question.

Le jeu se joue en round qui durent 20 secondes chacun. À chaque round, le premier joueur qui réponds correctement à la question posée gagne 10 points. Puis les autres joueurs qui répondent correctement gagnent `max(10 - (time(NULL) - time_of_first_response), 1)` points. Le jeu s'arrête lorsqu'un joueur a atteint 100 points.

# Avertissement
À cause d'un sérieux manque de temps due à des facteurs extérieurs, certaines fonctionnalitées ont du être annulées. Ainsi, vous verrez des `TODO` tout au long du code qui sont les vestiges des choses qui n'ont pas pu être implémentées à temps.

# Compilation

## Complier le serveur
Le serveur est dépendant de Sqlite pour stocker la base de données des questions, ainsi le programme est assez lourd (1.3MB).
Pour le compiler, faire:
```bash
cd server
make
```

L'exécutable s'appelle `popsauce-server`.

## Compiler le client

TODO

# Utilisation
## Utilisation du serveur

Pour démarrer le serveur, faire:
```bash
./popsauce-server
```

Vous remarquerez cependant qu'il n'y a pas de questions dans la base de données si c'est la première fois que vous allumez le serveur.

Pour ajouter des questions à la base de données, faire:
```bash
./popsauce-server add [txt/img]
```

Vous pouvez normalement ajouter des questions contenant des supports textuels ou avec des images.
Les supports sont des informations additionnelles qui permettant de répondre à la question.

Par exemple si la question est `Qui chante cette chanson?`, le support ressemble surement à 

```txt
Les sirènes du port d'Alexandrie
Chantent encore la même mélodie wowo
La lumière du phare d'Alexandrie
Fait naufrager les papillons de ma jeunesse. 
```

Vous remarquerez que ces questions ont souvent plusieurs réponses valides, ici: `Claude François ou Cloclo` par exemple.
Ainsi, pour marquer ces réponses différentes, vous pourrez séparer les réponses valides par une virgule.

Remarquez que ces réponses seront formattées pour enlever les majuscules, les espaces et les accents / caractères spéciaux.
Ainsi `claudefrancois` sera une réponse valide.

**IMPORTANT:** Par manque de temps, il n'a pas été possible d'ajouter le format image, donc la seule commande valie est `./popsauce-server add txt`

# Architecture de l'application

L'application est capable de créer et de faire jouer plusieurs 'lobbies' simultanément à des parties différentes.
Ainsi, lorsqu'un client se connecte au serveur, il lui envoit son nom d'utilisateur et son 'uuid' qui servira au
serveur pour l'identifier.
L'utilisateur a se choix de créer un lobby ou d'un rejoindre un déjà actif. Lorsqu'il rejoint un lobby, il reçoit
les informations des joueurs du lobby (id , pseudo et score actuel). Pour des raisons de prévention de triche, les id
reçue à ce moment là et l'uuid sont deux nombres différents. En effet, pour prouver qu'un joueur envoit bien un paquet,
il communique son uuid dans chacun des messages qu'il envoit au serveur. Puisqu'il est le seul à connaître cette uuid, elle
sert d'authentification.

Les lobbies ont globalement deux états: en attente et en jeu. Lorsqu'un lobby est en attente, le propriétaire a la possibilité
de lancer la partie en envoyant un message spécial au serveur. Le lobby passe alors peu après en état de jeu et la partie commence.
Une fois la partie terminée, le lobby revient en état d'attente. Il n'est supprimé que lorsqu'il n'y a plus de joueurs dedans.

## Communication Client/Serveur

De part la multitude de messages qu'il est possible d'envoyer du serveur à un client et vice-verça, un système de communication
a été créer pour faciliter le transfert de données.

On a donc introduit le concept de `Message`, qui contient l'uuid de la personne qui l'envoit, le type de message parmi une liste,
la taille du 'payload' et le 'payload' en lui même. Ces messages sont sérialisés avant d'être envoyés sur le réseau.
Une fois reçu, ils sont désérialisés pour les retransformer en Messages, ce qui permet à des fonctions auxilières de les traiter.

Ces fonctions se trouvent toutes dans `common.c` et `common.h`. Les principales sont:
- `serialize_message()`: Qui transforme un `Message` en une liste de bytes.
- `deserialize_message()`: Qui transforme une liste de bytes en `Message`.
- `send_message()`: Qui envoit une liste de byte à un serveur et qui retourne potentiellement une réponse du serveur.
- `receive_message()`: Qui retourne une liste de byte envoyée depuis `send_message()`

Comme ces fonctions sont nécessaires pour le client comme le serveur, ce code est commun aux deux programme. Cela permet d'éviter d'écrire deux fois le même code.

## Architecture du serveur

Le code du serveur se trouve dans `server/src` (sans compter `common.c`).
Les headers se trouvent dans `server/include` et les librairies externes dans `server/lib`.

### Le fichier main
Le fichier principal du serveur est `server/src/main.c`. Ce fichier permet de créer le serveur et de maintenir les connections.
On crée un thread à chaque connection de client pour gérer la communication `client -> serveur`.

Cependant, on crée aussi un thread par lobby pour gérer la communication `serveur -> client`. Ces threads permettent de 
broadcaster à tous les joueurs en même temps des informations, sans être bloqué par des évènements de l'autre thread.

Cette configuration permet d'envoyer et de recevoir des informations en même temps.

Il existe tout de même un risque avec `send_message()` qui peut retourner le mauvais message de réponse si un message a été broadcasté
entre l'envoi du message par `send_message()` et le processing de ce message par le serveur.
Cependant, les seuls messages qui attendent une réponse lors d'un `send_message()` sont envoyés lorsque le client n'est pas encore dans un lobby. Ainsi, il ne peut pas y avoir de confusion.

### Le fichier game_logic
Probablement le deuxième fichier le plus important du serveur. Ce fichier contient toute la logique pour jouer
au Popsauce.

À cause des problèmes de parallélisation, toutes les fonctions de ce fichier sont protégés par des mutex qui empêchent
les données de se retrouver dans un état invalide.

Un thread supplémentaire est créé pour chaque jeu dans l'état en cours. Ce thread permet de gérer les timings du jeu sans
freeze le reste du programme pour les autres lobbies.

### Le fichier questions
Ce fichier contient l'interface du serveur avec sqlite.
Il permet de créer et gérer les différentes bases de données dont le jeu a besoin pour fonctionner.
La base de données a une table Questions qui contient toutes les questions du jeu.

Comme on veut poser des questions uniques pour chaque partie on crée aussi une table
par lobby afin de se souvenir des questions que l'on a déjà posé dans la partie.
Ces tables sont vidées lorsque le lobby repasse en état d'attente et détruites lorsque le lobbie est effacé.

### Le fichier message_queue
Ce fichier contient une structure de données de file afin d'assurer le traitement et l'envoi des messages dans l'ordre
dans lequel ils ont été designé pour être envoyés.
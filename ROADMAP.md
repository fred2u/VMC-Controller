# Feuille de route

Le VMC Controller fonctionne au quotidien, mais reste un **prototype**. Ce document liste ce qu'il manque pour en faire une version plus aboutie, facile à reproduire et à maintenir. Il sert de base à une éventuelle version 2, la version actuelle n'étant pas destinée à évoluer.

Les points sont regroupés par thème. La priorité indique l'intérêt pour quelqu'un qui reproduit le projet.

| Priorité | Sens |
|---|---|
| 🔴 Haute | Manque gênant à l'installation ou en cas de panne |
| 🟠 Moyenne | Amélioration nette du confort ou de la fiabilité |
| 🟢 Basse | Finition |
| ✅ | Fait |

---

## 1. Fonctions de la télécommande

### 🔴 Mode appairage de la télécommande

**Situation actuelle** : pour appairer la télécommande avec la VMC (après un remplacement de la VMC ou une réinitialisation, par exemple), il faut appuyer **en même temps** sur `1` et `Auto`. Le firmware n'envoie qu'un appui à la fois, donc il faut ouvrir le boîtier et ponter les contacts à la main.

**À faire** :

- Ajouter une route `/pair` (et un bouton dans l'interface, éventuellement caché dans une page « Maintenance ») qui met GPIO2 (`1`) et GPIO5 (`Auto`) à l'état bas en même temps, jusqu'à ce que la LED de la télécommande clignote. La durée exacte est à déterminer par essai : le manuel dit seulement « jusqu'à ce que la LED clignote ».
- Afficher la procédure côté VMC dans l'interface : débrancher la VMC 5 secondes, la rebrancher, puis lancer l'appairage dans les 3 minutes.
- Ajouter de la même façon la **réinitialisation de la télécommande** (`Défaut` et `Timer` appuyés ensemble pendant 3 secondes).
- Rendre `sendCommand` capable d'appuyer sur plusieurs boutons à la fois et de tenir l'appui plus longtemps, ce qui servira aux deux fonctions.

### 🟠 Retour d'information sur les modes Max

- Les commandes Max sont ignorées sans aucun message quand le mode courant est High. Il faudrait au moins le signaler dans la réponse HTTP et dans l'interface. Ce comportement est peut‑être voulu (la VMC est déjà en position élevée), mais ce n'est écrit nulle part.
- Les routes de commande répondent `200` avant d'exécuter la commande. Une réponse qui donne le mode obtenu, ou une erreur, serait plus utile à Home Assistant.

---

## 2. Matériel

### 🔴 Bouton de réinitialisation usine sur le boîtier

**Situation actuelle** : aucun bouton n'est accessible boîtier fermé. En cas de changement de box Internet ou de mot de passe Wi‑Fi, il faut ouvrir le boîtier et reflasher l'ESP.

**À faire** :

- Prévoir un bouton accessible de l'extérieur, encastré ou au bout d'un trombone pour éviter les appuis accidentels. Il peut être câblé sur une broche libre (GPIO13 par exemple) ou sur le bouton `FLASH` (GPIO0) de la carte.
- Comportement proposé :
  - appui court : redémarrage ;
  - appui long (environ 10 secondes) : effacement des réglages Wi‑Fi enregistrés et retour en mode configuration (voir la partie Wi‑Fi plus bas) ;
  - éventuellement, réinitialisation de la télécommande (voir plus haut).
- Signaler chaque étape avec la LED (clignotement rapide pendant l'appui long, par exemple).
- Prévoir l'emplacement du bouton dans le boîtier imprimé en 3D.

### 🟠 Version plus compacte

**Situation actuelle** : le montage est un prototype, avec une carte de développement ESP‑Mx DevKit, un adaptateur FFC, une nappe et un PCB maison de 50 × 50 mm dont l'ajustement n'était pas parfait. Le tout tient dans un boîtier d'environ 62 × 62 × 42 mm.

**Pistes pour une version 2** :

- **Un seul PCB** qui porte directement :
  - un module ESP nu (ESP‑12F ou ESP8285) à la place de la carte de développement ;
  - un connecteur FPC 8 broches au pas de 0,5 mm soudé sur la carte, pour brancher la nappe de la télécommande sans adaptateur ;
  - une prise USB (de préférence USB‑C) et un régulateur 3,3 V ;
  - le bouton de réinitialisation et une LED d'état.
- **Empilement** : poser la carte de la télécommande sur le nouveau PCB, ou l'inverse, plutôt que côte à côte.
- **Empreintes à vérifier** avant de commander : mesurer les cotes réelles des composants (entraxes, position des connecteurs) et imprimer le PCB à l'échelle 1:1 sur papier pour vérifier l'ajustement.
- **Programmation** : prévoir un connecteur série (RX, TX, GND, GPIO0, RST) pour le premier flash, puis passer à l'OTA.
- **Boîtier** : le redessiner autour du nouveau PCB et publier le fichier source (STEP ou Fusion 360) en plus des STL, pour qu'il puisse être modifié.

### 🟢 Broches de démarrage

GPIO2 et GPIO15 servent aussi à choisir le mode de démarrage de l'ESP8266. Si le circuit de la télécommande tire l'une de ces broches dans le mauvais sens au démarrage, l'ESP ne démarre pas. Sur une nouvelle carte, utiliser de préférence des broches sans rôle au démarrage (GPIO4, 5, 12, 13, 14), ou vérifier ce point.

---

## 3. Logiciel

### 🔴 Gestion du Wi‑Fi par une librairie

**Situation actuelle** : le nom et le mot de passe du réseau sont écrits en dur dans `include/wifi.h` et compilés dans le firmware. Pour changer de réseau, il faut recompiler et reflasher. Si le réseau est indisponible, l'ESP ne propose aucune solution de secours.

**À faire** :

- Utiliser une librairie de configuration Wi‑Fi, par exemple [ESPAsyncWiFiManager](https://github.com/alanswx/ESPAsyncWiFiManager), compatible avec le serveur web asynchrone déjà utilisé, ou [WiFiManager](https://github.com/tzapu/WiFiManager).
- Au premier démarrage, ou si le réseau enregistré est introuvable, l'ESP ouvre son propre point d'accès (par exemple `VMC-Setup`) avec un portail captif pour choisir le réseau et saisir le mot de passe.
- Les identifiants sont enregistrés dans la mémoire flash et effacés par le bouton de réinitialisation usine.
- Supprimer `include/wifi.h` et les instructions correspondantes du README.
- Annoncer l'ESP sur le réseau par mDNS (`vmc.local`), pour ne pas dépendre du routeur pour la résolution du nom.

### 🟠 Mises à jour par Wi‑Fi (OTA)

Ajouter ArduinoOTA ou une page de mise à jour web (`/update`), pour mettre à jour le firmware et l'interface sans ouvrir le boîtier ni brancher de câble.

### 🟠 Intégration Home Assistant native

Aujourd'hui, l'intégration passe par une configuration `rest_command` manuelle. Une intégration MQTT avec la découverte automatique de Home Assistant créerait directement un appareil « VMC » avec ses boutons et un capteur de mode. Elle permettrait aussi de connaître l'état sans interroger `/mode` en boucle.

### 🟠 Fiabilité des commandes

- **Commandes perdues** : une seule commande est mémorisée et elle est effacée à la fin de son exécution. Une commande reçue pendant l'envoi d'une autre (jusqu'à environ 3 secondes pour Max 60 min) est perdue. Il faudrait une petite file d'attente, ou au moins refuser la requête avec un code `409` ou `503`.
- **Variables partagées** : la commande en cours est une `String` écrite par le serveur asynchrone et lue par `loop()` sans protection. Une variable simple (un entier ou une énumération) marquée `volatile` serait plus sûre.
- **Temporisations bloquantes** : `loop()` utilise `delay()` pendant l'envoi des impulsions. Une machine à états basée sur `millis()` garderait l'ESP réactif.
- **Durée des modes Max** : la fin du mode Max est calculée avec `time(0)`, sans synchronisation NTP. Il serait plus simple et plus sûr d'utiliser `millis()`.

### 🟢 État affiché

Le mode affiché est celui de la dernière commande envoyée par l'ESP, et il n'est pas lu sur la VMC. Une commande envoyée par une autre télécommande, ou le retour en mode auto après une coupure de courant, ne sont pas vus. C'est une limite du montage, puisque la télécommande ne reçoit rien. Il faut au moins l'indiquer dans l'interface (« dernier mode envoyé »).

✅ L'interface affiche maintenant « Last mode sent » au lieu de « Current mode ». La limite elle‑même reste.

### 🟢 Sécurité

Aucune authentification n'est en place. C'est acceptable sur un réseau local de confiance, mais il faudrait au minimum une authentification HTTP basique en option, surtout si l'OTA est ajouté, puisqu'elle permettrait de remplacer le firmware.

---

## 4. Interface web

- ✅ ~~Uniformiser la langue : les boutons sont en anglais (« Low », « Medium », « High »), sauf « Automatique », et la page est déclarée en français.~~ Tout est en anglais : « Automatique » devient « Auto » (aussi dans la réponse de `/mode`) et la page est déclarée en anglais.
- ✅ ~~Mettre en évidence le mode actif sur le bouton correspondant, en plus du texte « Current mode ».~~ Le bouton du mode actif est rempli.
- ✅ ~~Corriger une faute de frappe dans `data/style.css` : `margin: 0 10px O O;` utilise la lettre `O` au lieu du chiffre `0`, ce qui rend la règle invalide.~~ Remplacé par `margin: 0 10px;`.
- ✅ Afficher le mode dès l'ouverture de la page, sans attendre le premier rafraîchissement de 2 secondes.
- ✅ Corriger la balise `<meta>` de `index.html`, où `charset` était collé à l'attribut `viewport` et donc ignoré.
- 🟢 Ajouter une page « Maintenance » : appairage, réinitialisation, informations système (version, adresse IP, force du signal Wi‑Fi, temps de fonctionnement).

---

## 5. Projet et documentation

- 🟠 Ajouter un schéma électrique, par exemple exporté depuis EasyEDA, en plus du PCB.
- ✅ ~~Ajouter une vérification de compilation automatique (GitHub Actions avec PlatformIO), pour s'assurer que le firmware compile toujours avec les versions récentes des librairies.~~ Fait avec les tests unitaires (`.github/workflows/ci.yml`).
- ✅ ~~Fixer la version de la librairie ESP Async WebServer dans `platformio.ini`. Sans version, une mise à jour de la librairie peut casser la compilation.~~ Fait : les dépôts GitHub sont épinglés sur un commit précis.
- 🟢 Numéroter les versions du firmware (tags Git) et afficher la version dans l'interface.

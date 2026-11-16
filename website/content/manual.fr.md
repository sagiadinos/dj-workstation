---
title: Manuel
description: Utiliser DJ-Workstation – lecteurs, crossfader, bibliothèque, listes de lecture, contrôleurs et réglages.
layout: manual
---

DJ-Workstation est d’abord conçu pour les écrans tactiles : chaque commande fonctionne au doigt, et la souris de la même
façon. Laissez la souris sur une commande et une courte aide apparaît. Les commandes du lecteur gardent dans toutes les
langues les mots anglais du DJ (Load, CUE, KEY, Pitch).

## Sommaire {#contents}

- [Premier démarrage](#first-start)
- [Le lecteur](#the-player)
- [Formes d’onde des beats et crossfader](#beat-waveforms-and-crossfader)
- [La bibliothèque](#the-library)
- [Listes de lecture](#playlists)
- [Fenêtres et dispositions](#windows-and-layouts)
- [Raccourcis clavier](#keyboard-shortcuts)
- [Contrôleurs MIDI](#midi-controllers)
- [Réglages](#settings)

## Premier démarrage {#first-start}

Tant que la bibliothèque n’a pas de dossier de musique, DJ-Workstation en demande un au démarrage. Il lit ensuite toutes
les pistes de ce dossier et de ses sous-dossiers (MP3, Ogg Vorbis, FLAC et WAV) avec leurs tags. Vous ajoutez d’autres
dossiers plus tard avec **Fichier > Ajouter un dossier de musique…**.

Sans disposition enregistrée, le programme ouvre deux lecteurs avec les formes d’onde des beats entre eux et le
crossfader en dessous.

## Le lecteur {#the-player}

{{< callouts name="player" alt="Un lecteur avec des repères numérotés sur ses commandes" >}}

1. **Load** charge dans ce lecteur la piste choisie dans la bibliothèque. Vous pouvez aussi toucher deux fois une piste
   de la bibliothèque (elle va dans un lecteur libre) ou faire glisser un fichier du gestionnaire de fichiers sur le
   lecteur. Pendant la lecture, Load est grisé : une piste en cours n’est jamais remplacée.
2. **− / +** ralentissent ou accélèrent la piste tant que vous les tenez, pour remettre deux pistes en phase.
3. **KEY** active le keylock : le tempo change avec le fader de pitch, la tonalité reste.
4. **Time** bascule l’affichage entre temps écoulé et temps restant.
5. **BPM** est le tempo de la piste, pitch compris.
6. **La forme d’onde** montre toute la piste ; la partie déjà jouée devient jaune. Touchez-la pour y sauter. Sur un
   lecteur arrêté, vous entendez une courte boucle à l’endroit touché et trouvez ainsi un passage à l’oreille ; CUE y
   pose alors le point de cue.
7. **L’état** du lecteur : chargé, au cue, pause ou lecture.
8. **CUE** fonctionne comme sur un lecteur CD : à l’arrêt, il pose le point de cue à la position actuelle ; en lecture,
   il revient au point de cue et s’arrête ; maintenu, il joue depuis le point de cue jusqu’à ce que vous lâchiez.
9. **Play** lance la piste et la met en pause.
10. **Le fader de pitch** change le tempo jusqu’à ±10 % par pas de 0,02 %. Comme sur une platine, vers le bas c’est
    plus rapide. Un double toucher le remet à 0.
11. **Vol** est le volume de la voie ; l’indicateur dans sa glissière montre le niveau.
12. **Pré-écoute** envoie le lecteur au casque ; son voyant est allumé tant qu’elle est active.
13. **Head** est le volume du casque.
14. **Artiste et titre** de la piste chargée.
15. **Cue-Load** : activé, une piste chargée démarre au dernier point de cue que vous lui avez donné. Une piste sans
    point de cue démarre à son premier son plutôt que dans le silence qui le précède.

## Formes d’onde des beats et crossfader {#beat-waveforms-and-crossfader}

{{< callouts name="mixer" alt="Formes d’onde des beats et crossfader avec des repères numérotés" >}}

1. **Les formes d’onde des beats** montrent six secondes des lecteurs des côtés A et B : basses en rouge, médiums en
   vert, aigus en bleu.
2. **La ligne fixe** est l’endroit joué en ce moment. Quand les beats des deux pistes la touchent ensemble, les pistes
   sont en phase.
3. **Ce bouton** fait passer les pistes de la verticale à l’horizontale et inversement.
4. **Les deux listes déroulantes** choisissent le lecteur du côté A et celui du côté B.
5. **Zéro** met le crossfader au milieu.
6. **L’indicateur** montre le niveau du master.
7. **A** et **B** sautent de ce côté tant que vous les tenez et reviennent quand vous lâchez.
8. **Le crossfader** passe du côté A au côté B.
9. **Fondu maintenant !** passe tout seul de l’autre côté.
10. **Durée du fondu (s)** règle la durée de ce fondu.

## La bibliothèque {#the-library}

{{< callouts name="library" alt="La bibliothèque avec des repères numérotés sur la barre de menus, la recherche, les filtres et la liste" >}}

1. **La barre de menus** : Fichier, Liste de lecture, Réglages, Affichage et Disposition.
2. **Recherche** trouve les pistes dès la frappe, dans tous les champs à la fois. Ce que vous tapez alors qu’aucun champ
   de texte n’est sélectionné arrive aussi ici : il suffit de taper.
3. **Les filtres de colonne** cherchent chacun dans une colonne : artiste, titre, genre et année. `*` remplace
   n’importe quel texte, `?` un caractère.
4. **Un en-tête de colonne** trie la liste selon cette colonne.
5. **La liste des pistes** : touchez une piste pour la sélectionner, deux fois pour la charger dans un lecteur libre.
6. **Le nombre de pistes** trouvées, et ce que fait la bibliothèque (lecture des fichiers, analyse).

Un clic droit sur une piste offre davantage :

- **Modifier les tags** change artiste, titre, genre et année dans le fichier et dans la bibliothèque. Les tags d’une
  piste en cours de lecture ne peuvent pas être modifiés.
- **Relire** relit les tags du fichier et l’analyse à nouveau.

Le tempo (BPM) et la forme d’onde d’une piste sont calculés à son premier chargement. **Fichier > Analyser toutes les
pistes** le fait pour toute la bibliothèque en arrière-plan, pendant que vous continuez à travailler.

**Fichier > Réanalyser la bibliothèque** cherche les fichiers nouveaux, modifiés et supprimés. Les fichiers disparus
sont comptés ; **Retirer les manquants** les enlève de la bibliothèque.

## Listes de lecture {#playlists}

Ouvrez la liste de lecture avec **Affichage > Fenêtre de la liste de lecture**. **Nouvelle** crée une liste,
**Ajouter à la liste** dans la bibliothèque ajoute la piste sélectionnée, **Monter** et **Descendre** changent l’ordre et
**Retirer** enlève une piste. Les listes sont gardées dans la bibliothèque et s’importent et s’exportent en fichiers M3U
(**Importer**, **Exporter**).

## Fenêtres et dispositions {#windows-and-layouts}

Chaque lecteur, le crossfader, les formes d’onde des beats et la liste de lecture peuvent avoir leur propre fenêtre,
librement déplaçable et redimensionnable, aussi sur un autre écran. **Affichage > Mode fenêtre unique** les réunit plutôt
dans une seule fenêtre (au prochain démarrage). **Affichage > Nouveau lecteur** ouvre un lecteur de plus ; il n’y a pas
de limite fixe. **Affichage > Plein écran** (Ctrl+F) remplit l’écran, pour les terminaux tactiles.

Dans la fenêtre unique, **Disposition > Modifier la disposition** permet de faire glisser une partie pour la déplacer,
ou son coin inférieur droit pour la redimensionner ; les bords s’aimantent aux voisins. **Disposition > Enregistrer la
disposition** la garde pour le prochain démarrage, **Disposition > Rétablir la disposition par défaut** revient à deux
lecteurs avec le crossfader.

**Affichage > Couleurs pour la lumière du jour** passe à des couleurs claires pour les pièces lumineuses ou le soleil.

Pendant la lecture d’une piste, **Fichier > Quitter** demande d’abord confirmation, pour qu’un set ne s’arrête pas par
erreur.

## Raccourcis clavier {#keyboard-shortcuts}

Ils agissent sur le lecteur qui a le focus clavier et utilisent toujours la touche Alt de gauche (Option sur Mac), pour
qu’une recherche tapée n’atteigne jamais un lecteur.

| Touches | Action |
|---|---|
| Alt+P | Lecture / pause |
| Alt+C | Cue (agit tant que maintenu) |
| Alt+L | Charger la piste sélectionnée |
| Alt+← / Alt+→ | Un pas en arrière / en avant (avec Maj : 10 secondes) |
| Alt+↑ / Alt+↓ | Pitch ±0,02 % (avec Maj : ±0,1 %) |
| Ctrl+F | Plein écran |

## Contrôleurs MIDI {#midi-controllers}

Les contrôleurs se branchent et se débranchent pendant que le programme tourne. **Réglages > Périphériques…** attribue un
mapping à chaque contrôleur. Un mapping est un petit fichier TOML qui relie boutons, faders et jog wheels du contrôleur à
lecture, cue, chargement, pitch, volume, crossfader et plus, et allume ses LED. Le format est décrit dans
[mappings/README.md](https://github.com/sagiadinos/dj-workstation/blob/main/mappings/README.md) (en anglais).

**Importer un mapping Mixxx…** transforme un mapping Mixxx (.midi.xml) en l’un des nôtres. Ses affectations directes
sont reprises ; ce que Mixxx fait par script est laissé de côté et compté.

Le Hercules DJControl Starlight est reconnu à son nom, avec sa carte son : master sur les sorties 1/2, casque sur 3/4.
Ses jog wheels font varier le pitch quand on les tourne par le bord et scratchent quand on les touche sur le dessus.

## Réglages {#settings}

**Réglages > Périphériques…** : la sortie audio, stéréo ou quatre canaux (master sur 1/2, casque sur 3/4), la latence
(plus courte réagit plus vite, plus longue protège des coupures) et les mappings des contrôleurs. Les réglages audio
prennent effet au prochain démarrage.

**Réglages > Son…** : la qualité du keylock, Élevée (par défaut) ou Standard pour les ordinateurs lents.

**Réglages > Langue…** : anglais, allemand, français, espagnol, grec ou russe ; le changement est immédiat.

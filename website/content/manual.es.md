---
title: Manual
description: Cómo usar DJ-Workstation – reproductores, crossfader, biblioteca, listas, controladores y ajustes.
layout: manual
---

DJ-Workstation está hecho ante todo para pantallas táctiles: cada control funciona con el dedo, y el ratón igual. Deja
el ratón sobre un control y aparece una breve ayuda. Los controles del reproductor conservan en todos los idiomas las
palabras inglesas del DJ (Load, CUE, KEY, Pitch).

## Contenido {#contents}

- [Primer inicio](#first-start)
- [El reproductor](#the-player)
- [Formas de onda de beats y crossfader](#beat-waveforms-and-crossfader)
- [La biblioteca](#the-library)
- [Listas](#playlists)
- [Ventanas y disposiciones](#windows-and-layouts)
- [Atajos de teclado](#keyboard-shortcuts)
- [Controladores MIDI](#midi-controllers)
- [Ajustes](#settings)

## Primer inicio {#first-start}

Mientras la biblioteca no tenga carpeta de música, DJ-Workstation pide una al iniciar. Después lee todas las pistas de
esa carpeta y sus subcarpetas (MP3, Ogg Vorbis, FLAC y WAV) junto con sus etiquetas. Más adelante añades otras carpetas
con **Archivo > Añadir carpeta de música…**.

Sin una disposición guardada, el programa abre dos reproductores con las formas de onda de beats entre ellos y el
crossfader debajo.

## El reproductor {#the-player}

{{< callouts name="player" alt="Un reproductor con marcas numeradas en sus controles" >}}

1. **Load** carga en este reproductor la pista elegida en la biblioteca. También puedes tocar dos veces una pista de la
   biblioteca (va a un reproductor libre) o arrastrar un archivo desde el gestor de archivos al reproductor. Mientras
   el reproductor suena, Load aparece atenuado: una pista que suena nunca se sustituye.
2. **− / +** frenan o aceleran la pista mientras los mantienes pulsados, para volver a cuadrar dos pistas.
3. **KEY** activa el keylock: el tempo cambia con el fader de pitch, la tonalidad se mantiene.
4. **Time** cambia la pantalla entre tiempo transcurrido y restante.
5. **BPM** es el tempo de la pista, con el pitch incluido.
6. **La forma de onda** muestra toda la pista; la parte ya sonada se vuelve amarilla. Tócala para saltar allí. En un
   reproductor parado oyes un bucle corto en el punto que tocas y así encuentras un pasaje de oído; CUE fija entonces
   ahí el punto de cue.
7. **El estado** del reproductor: cargado, en cue, pausa o sonando.
8. **CUE** funciona como en un reproductor de CD: parado, fija el punto de cue en la posición actual; sonando, vuelve
   al punto de cue y se para; mantenido, suena desde el punto de cue hasta que lo sueltas.
9. **Play** inicia y pausa la pista.
10. **El fader de pitch** cambia el tempo hasta ±10 % en pasos de 0,02 %. Como en un tocadiscos, hacia abajo es más
    rápido. Un doble toque lo devuelve a 0.
11. **Vol** es el volumen del canal; el medidor en su ranura muestra el nivel.
12. **Escucha previa** manda el reproductor a los auriculares; su luz está encendida mientras lo hace.
13. **Head** es el volumen de los auriculares.
14. **Artista y título** de la pista cargada.
15. **Cue-Load**: activado, una pista cargada empieza en el último punto de cue que le pusiste. Una pista sin punto de
    cue empieza en su primer sonido en vez de en el silencio anterior.

## Formas de onda de beats y crossfader {#beat-waveforms-and-crossfader}

{{< callouts name="mixer" alt="Formas de onda de beats y crossfader con marcas numeradas" >}}

1. **Las formas de onda de beats** muestran seis segundos de los reproductores de los lados A y B: graves en rojo,
   medios en verde, agudos en azul.
2. **La línea fija** es el punto que suena ahora. Cuando los beats de ambas pistas la tocan a la vez, las pistas van
   cuadradas.
3. **Este botón** gira las pistas de vertical a horizontal y al revés.
4. **Las dos listas desplegables** eligen qué reproductor está en el lado A y cuál en el lado B.
5. **Cero** pone el crossfader en el centro.
6. **El medidor** muestra el nivel del master.
7. **A** y **B** saltan a ese lado mientras los mantienes y vuelven al soltarlos.
8. **El crossfader** mezcla del lado A al lado B.
9. **¡Fundir ahora!** pasa solo al otro lado.
10. **Duración del fundido (s)** fija cuánto dura ese fundido.

## La biblioteca {#the-library}

{{< callouts name="library" alt="La biblioteca con marcas numeradas en la barra de menús, la búsqueda, los filtros y la lista" >}}

1. **La barra de menús**: Archivo, Lista, Ajustes, Vista y Diseño.
2. **Buscar** encuentra pistas mientras escribes, en todos los campos a la vez. Lo que escribes sin ningún campo de
   texto seleccionado también llega aquí: basta con empezar a escribir.
3. **Los filtros de columna** buscan cada uno en una columna: artista, título, género y año. `*` sustituye cualquier
   texto, `?` un carácter.
4. **Un encabezado de columna** ordena la lista por esa columna.
5. **La lista de pistas**: toca una pista para seleccionarla, dos veces para cargarla en un reproductor libre.
6. **El número de pistas** encontradas y lo que hace la biblioteca (leyendo archivos, analizando).

Un clic derecho en una pista ofrece más:

- **Editar etiquetas** cambia artista, título, género y año en el archivo y en la biblioteca. Las etiquetas de una
  pista que está sonando no se pueden cambiar.
- **Volver a leer** lee de nuevo las etiquetas del archivo y lo analiza otra vez.

El tempo (BPM) y la forma de onda de una pista se calculan la primera vez que la cargas. **Archivo > Analizar todas las
pistas** lo hace para toda la biblioteca en segundo plano, mientras sigues trabajando.

**Archivo > Volver a analizar la biblioteca** busca archivos nuevos, cambiados y borrados. Los archivos que faltan se
cuentan; **Quitar las que faltan** los saca de la biblioteca.

## Listas {#playlists}

Abre la lista con **Vista > Ventana de la lista**. **Nueva** crea una lista, **Añadir a la lista** en la biblioteca
añade la pista seleccionada, **Subir** y **Bajar** cambian el orden y **Quitar** saca una pista. Las listas se guardan
en la biblioteca y se importan y exportan como archivos M3U (**Importar**, **Exportar**).

## Ventanas y disposiciones {#windows-and-layouts}

Cada reproductor, el crossfader, las formas de onda de beats y la lista pueden tener su propia ventana, que se mueve y
redimensiona libremente, también a otra pantalla. **Vista > Modo de ventana única** los reúne en una sola ventana (desde
el próximo inicio). **Vista > Nuevo reproductor** abre otro reproductor; no hay un límite fijo. **Vista > Pantalla
completa** (Ctrl+F) llena la pantalla, para terminales táctiles.

En la ventana única, **Diseño > Editar la disposición** permite arrastrar una parte para moverla, o su esquina inferior
derecha para cambiar su tamaño; los bordes se ajustan a los vecinos. **Diseño > Guardar la disposición** la conserva
para el próximo inicio, **Diseño > Restablecer la disposición** vuelve a dos reproductores con el crossfader.

**Diseño > Guardar la disposición como…** escribe la disposición en un archivo propio, por ejemplo uno por local o por
pantalla; **Diseño > Cargar una disposición…** la recupera al momento y la conserva para el próximo inicio. Una
disposición hecha en el otro modo de ventanas se muestra desde el próximo inicio.

**Vista > Colores para luz diurna** cambia a colores claros para salas luminosas o luz del sol.

Mientras suena una pista, **Archivo > Salir** pregunta primero, para que una sesión no termine por accidente.

## Atajos de teclado {#keyboard-shortcuts}

Actúan sobre el reproductor que tiene el foco del teclado y usan siempre la tecla Alt izquierda (Option en Mac), para
que una búsqueda escrita nunca llegue a un reproductor.

| Teclas | Acción |
|---|---|
| Alt+P | Reproducir / pausa |
| Alt+C | Cue (actúa mientras se mantiene) |
| Alt+L | Cargar la pista seleccionada |
| Alt+← / Alt+→ | Un paso atrás / adelante (con Mayús: 10 segundos) |
| Alt+↑ / Alt+↓ | Pitch ±0,02 % (con Mayús: ±0,1 %) |
| Ctrl+F | Pantalla completa |

## Controladores MIDI {#midi-controllers}

Los controladores se conectan y desconectan mientras el programa funciona. **Ajustes > Dispositivos…** asigna un mapeo a
cada controlador. Un mapeo es un pequeño archivo TOML que une botones, faders y jog wheels del controlador con
reproducir, cue, cargar, pitch, volumen, crossfader y más, y enciende sus LED. El formato se describe en
[mappings/README.md](https://github.com/sagiadinos/dj-workstation/blob/main/mappings/README.md) (en inglés).

**Importar mapeo de Mixxx…** convierte un mapeo de Mixxx (.midi.xml) en uno nuestro. Sus asignaciones directas se
adoptan; lo que Mixxx hace con scripts se deja fuera y se cuenta.

El Hercules DJControl Starlight se reconoce por su nombre, con su tarjeta de sonido: master en las salidas 1/2,
auriculares en 3/4. Sus jog wheels varían el pitch al girarlos por el borde y hacen scratch al tocarlos por arriba.

## Ajustes {#settings}

**Ajustes > Dispositivos…**: la salida de audio, estéreo o cuatro canales (master en 1/2, auriculares en 3/4), la
latencia (más corta reacciona antes, más larga protege de cortes) y los mapeos de los controladores. Los ajustes de
audio se aplican en el próximo inicio.

**Ajustes > Sonido…**: la calidad del keylock, Alta (por defecto) o Estándar para ordenadores lentos.

**Ajustes > Idioma…**: inglés, alemán, francés, español, griego o ruso; el cambio se ve al momento.

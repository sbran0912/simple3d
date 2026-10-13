# Build-Prozess

Dieses Projekt baut die Modulbibliothek **lib3d** zu einer statischen
Bibliothek `lib/lib3d.a` und linkt daraus die Anwendungen in `apps/`.

> Für die Benutzung der Bibliothek (Objekte mit/ohne Body, Rendermethoden)
> siehe [`docs/library.md`](library.md).

## Verzeichnisstruktur

```
.
├── Makefile          Build-Regeln (Bibliothek, Apps, Tests, Start)
├── lib/              Ziel der statischen Bibliothek
│   ├── lib3d.a       erzeugte statische Bibliothek (ignoriert von git)
│   └── src/          alle Quellen (*.cpp) und Header (*.hpp) der Bibliothek
├── apps/             Anwendungen, je App ein Unterordner mit main.cpp
│   ├── vorlage/main.cpp
│   ├── beispiel/main.cpp
│   ├── instanzen/main.cpp
│   └── ray/main.cpp
├── tests/            Unit-Test der Bibliothek
│   └── lib3d_test.cpp
├── build/            Objektdateien (*.o) – Zwischenerzeugnisse
└── bin/              fertig gelinkte Programme
```

Die Bibliothek wird aus **allen** `lib/src/*.cpp` gebaut. Eine neue
Quelldatei muss also nur in `lib/src/` abgelegt werden – das `Makefile`
findet sie automatisch über eine Wildcard. Dasselbe gilt für neue Apps:
Ein Unterordner in `apps/` mit `main.cpp` genügt, sofern der App-Name in
`APP_NAMES` im Makefile ergänzt wird (siehe unten).

## Voraussetzungen

- `g++` mit C++20
- `make`
- `ar` (binutils)
- GLFW3 + OpenGL (`libglfw3-dev`, `libgl-dev`) – vorhanden, wenn
  `pkg-config --cflags glfw3` etwas ausgibt

## Targets

| Befehl | Wirkung |
| --- | --- |
| `make lib` | Baut **nur** die statische Bibliothek `lib/lib3d.a`. |
| `make` / `make all` | Baut Bibliothek **und** alle Apps (Standardziel). |
| `make apps` | Baut und linkt alle Apps (Bibliothek wird bei Bedarf mitgebaut). |
| `make bin/<name>` | Baut nur eine einzelne App, z. B. `make bin/ray`. |
| `make run APP=<name>` | Baut die App `<name>` und **startet** sie. |
| `make run-<name>` | Kurzform zum Starten, z. B. `make run-ray`. |
| `make test` | Baut den Unit-Test und führt ihn aus. |
| `make clean` | Löscht `build/`, `bin/` und `lib/lib3d.a`. |

Verfügbare App-Namen: `vorlage`, `beispiel`, `instanzen`, `ray`.

### Beispiele

```bash
make lib              # nur die Bibliothek bereitstellen
make                  # alles bauen
make apps             # alle Apps bauen und linken
make run APP=ray      # App "ray" bauen und starten
make run-ray          # dasselbe, kurz
make bin/instanzen    # nur "instanzen" bauen
make test             # Unit-Test ausführen
make clean            # aufräumen
```

Wird `make run` ohne `APP` aufgerufen, gibt das Makefile eine
Fehlermeldung mit der Liste der verfügbaren Apps aus.

## Wie gebaut wird

1. **Bibliothek:** Für jede `lib/src/*.cpp` entsteht die Objektdatei
   `build/lib/<name>.o` (kompiliert mit `-Ilib/src`). Anschließend werden
   alle Objekte mit `ar rcs lib/lib3d.a …` zur statischen Bibliothek
   archiviert.
2. **Apps:** Für jede `apps/<name>/main.cpp` entsteht
   `build/apps/<name>/main.o`. Gelinkt wird mit dem App-Objekt **zuerst**
   und der Bibliothek danach, damit der Linker die benötigten Symbole
   findet:

   ```bash
   g++ build/apps/<name>/main.o lib/lib3d.a -o bin/<name> -lglfw -lGL
   ```

   Links von statischen Bibliotheken müssen nach den Objekten stehen –
   daher die Reihenfolge `main.o` → `lib3d.a` → Systembibliotheken.
3. **Header:** Header und Quellen liegen gemeinsam in `lib/src/`. Der
   Include-Pfad wird über `-Ilib/src` gesetzt, daher lautet das Einbinden
   in Apps und Tests:

   ```cpp
   #include "lib3d.hpp"   // Gesamt-API der Bibliothek
   #include "bodies.hpp"  // Zusatzmodul (Faces/Raycasting)
   ```

   Innerhalb der Bibliothek werden sich gegenseitig einbindende Header mit
   ihrem reinen Dateinamen eingebunden (z. B. `#include "vec.hpp"`).

## Erweitern

### Neue Bibliotheks-Quelldatei

1. `.cpp` (und ggf. `.hpp`) in `lib/src/` ablegen.
2. Fertig – die Wildcard `lib/src/*.cpp` nimmt sie automatisch auf.
3. Optional in `lib/src/lib3d.hpp` einbinden, wenn sie Teil der
   Gesamt-API sein soll.

### Neue App

1. Ordner `apps/<name>/` mit `main.cpp` anlegen.
2. Im Makefile in der Zeile `APP_NAMES := …` den Namen ergänzen.
3. Bauen mit `make apps`, starten mit `make run APP=<name>` bzw.
   `make run-<name>`.

## Hinweise

- `build/`, `bin/` und `*.a` sind über `.gitignore` von der
  Versionsverwaltung ausgenommen.
- Überschreiben der Werkzeuge ist möglich, z. B.
  `make CXX=clang++ lib`.
- `CXXFLAGS`/`CPPFLAGS` können beim Aufruf ergänzt werden.

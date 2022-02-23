# S10auslesen

*S10auslesen* ist ein serverseitiges Tool, welches sich mit einem E3/DC S10 Hauskraftwerk verbindet und die abrufbaren Daten ausliest und maschinenlesbar bereitstellt.

## Zielplattform

Vom Autor wird *S10auslesen* auf einer x86_64 Plattform getestet und im Produktiveinsatz auf einem Debian stable verwendet. Ein Einsatz auf anderen Plattformen wie einem Raspberry Pi oder Arduino wurde bei der Entwicklung von *S10auslesen* berücksichtigt: Der Quellcode wurde mit dem Gedanken der Portierbarkeit in ISO-kompatiblem C geschrieben, wird aber mangels Zugang zu diesen Plattformen nicht getestet.

## Funktionsweise

*S10auslesen* nutzt das vom S10 Hauskraftwerk unterstützte Modbus-Protokoll, um eine Verbindung mit dem S10 aufzubauen und dessen bereitgestellte Daten auszulesen. Das S10 Hauskraftwerk aktualisiert die auslesbaren Messwerte einmal pro Sekunde, entsprechend liest *S10auslesen* während seiner Laufzeit einmal pro Sekunde die Daten aus. Die ausgelesenen Daten werden anschließend in einer JSON-Datei gespeichert, welche ebenfalls während der Laufzeit einmal pro Sekunde aktualisiert wird.

*S10auslesen* startet zum Stundenwechsel, also wenn die lokale Systemzeit hh:00:00 Uhr anzeigt und läuft diese Stunde lang durch. Nach der Gesamtlaufzeit von einer Stunde werden die 3600 ausgelesenen Messwerte in eine MySQL Datenbank eingetragen und somit für die spätere Verwendung archiviert. Dazu stellt *S10auslesen* eine Verbindung zu einer MySQL-kompatiblen Datenbank her und übergibt die Daten an diese, bevor es sich beendet.

*S10auslesen* ist für den Backend-Einsatz, also einem interaktionsfreien Einsatz auf einem ggfs. headless Server konzipiert. Daher ist in *S10auslesen* keine Interaktion mit einem Benutzer vorgesehen, es werden also keine Eingaben erwartet und auch keine Ausgaben generiert. Die Konfiguration von *S10auslesen* passiert bereits vor der Kompilierung, daher wird *S10auslesen* auch nur als Quellcode und nicht als Binärdatei veröffentlicht.

## Lizenz

![AGPLv3](https://www.gnu.org/graphics/agplv3-88x31.png) *S10auslesen* wird unter der **GNU Affero General Public License v3.0 or later** [AGPL-3.0-or-later](LICENSE.md) lizensiert

## Kompilieren

*S10auslesen* wird nur als Quellcode ausgeliefert und muss vor seinem Einsatz auf der Zielplattform kompiliert werden. Da *S10auslesen* in C geschrieben ist, wird also ein C Compiler benötigt.

### Schritt 1: Herunterladen und Anpassen

Da für *S10auslesen* keine Interaktionsmöglichkeit vorgesehen sind, muss die Zielkonfiguration bereits vor dem Kompilieren bearbeitet werden. Alle Konfigurationen sind dabei in der Datei src/Einstellungen.h zu machen.
Die meisten Werte sind Standardwerte, z.&nbsp;B. die Ports für die Verbindungen, und können so belassen werden, falls sie nicht ebenfalls auf den zugehörigen Servern geändert wurden. Einige Werte sind Plattform-spezifisch, z.&nbsp;B. ist bei SQL_SOCKET der Standard-Socket des MariaDB-Servers unter Debian11 voreingestellt.
Folgende Einstellungen sind allerdings benutzerabhängig und müssen auf jeden Fall vor dem Kompilieren angepasst werden:
| anzupassende Konstante | Typ | Beschreibung |
| ---- | ---- | ---- |
| S10_ADRESSE | string | Adresse des S10 Hauskraftwerks, z.&nbsp;B. seine IP |
| SQL_ADRESSE | string | Adresse des SQL-Servers, z.&nbsp;B. seine IP |
| SQL_USER | string | SQL-Benutzername, der für die Verbindung zum SQL-Server verwendet werden soll |
| SQL_PW | string | Passwort des SQL-Benutzers |
| SQL_DB | string | Datenbank auf dem SQL-Server, in der die Tabellen angelegt werden |
| JSON_FILE | string | Pfad und Dateiname, wohin die JSON-Datei geschrieben wird |

### Schritt 2: Abhängigkeiten auflösen

*S10auslesen* hängt für die Verbindungsherstellung von externen Bibliotheken ab, welche auf dem Zielsystem vorhanden sein müssen, damit das Programm ordentlich gelinkt werden kann.

#### libmodbus

Für die Verbindung mittels Modbus verwendet *S10auslesen* die Bibliothek libmodbus. Diese kann entweder direkt von ihrer [Webseite](https://libmodbus.org/) oder vom [Repository](https://github.com/stephane/libmodbus) heruntergeladen werden.
Wenn libmodbus in den Paketquellen ihres Betriebssystems vorhanden ist, ist es empfehlenswert es direkt von dort zu installieren.
Auf einem Debian Server ist das z.&nbsp;B. mittels folgendem Kommandozeilenbefehl möglich, der mit root-Rechten ausgeführt werden muss:
> apt-get install libmodbus-dev

*S10auslesen* nutzt nur die grundlegende Verbindungsfunktion, sodass auch antike libmodbus-Versionen funktionieren sollten, solange sie Modbus/TCP implementiert haben. Getestet wurde *S10auslesen* gegen den aktuell stabilen 3.0.x Zweig.

#### mysql

Als Datenbankverbindung wird MySQL von *S10auslesen* unterstützt, es wird der MariaDB Connector/C verwendet. Der kann direkt von der [MariaDB Webseite](https://mariadb.com/kb/en/mariadb-connector-c/) heruntergeladen werden.
Da der Connector in den Paketquellen sehr vieler Betriebssysteme vorhanden ist, ist es in diesem Fall empfehlenswert es direkt von dort zu installieren.
Auf einem Debian Server ist das z.&nbsp;B. mittels folgendem Kommandozeilenbefehl möglich, der mit root-Rechten ausgeführt werden muss:
> apt-get install libmariadb-dev

### Schritt 3: S10auslesen kompilieren

*S10auslesen* wird kompiliert, indem im Basis-Ordner - also dort wo auch die `Makefile` liegt - folgender Kommandozeilenbefehl ausgeführt wird:
> make

Das fertig kompilierte Programm wird im Verzeichnis `bin` ausgegeben. Dort liegt die ausführbare Datei `S10auslesen`, welche nun gestartet werden kann.

Falls auf dem Server eine Fehlermeldung ähnlich der folgenden erscheint
> command not found: make

dann sind die Kompilierwerkzeuge noch nicht installiert und müssen noch installiert werden.
Auf einem Debian Server ist das z.&nbsp;B. mittels folgendem Kommandozeilenbefehl möglich, der mit root-Rechten ausgeführt werden muss:
> apt-get install build-essential

## autmatisiertes Starten mittels Cron

*S10auslesen* hat eine Laufzeit von einer Stunde und startet von selbst im Moment, in dem eine neue Stunde anbricht.
Ein händisches Starten von *S10auslesen* ist daher nicht empfehlenswert. Auf praktisch allen unixartigen Betriebssystemen ist Cron installiert, das den automatischen Start von Befehlen ermöglicht.
Um das automatisierte Starten von *S10auslesen* einzurichten, legen wir daher einen Cronjob an. Dazu muss folgender Kommandozeilenbefehl ausgeführt werden:
> crontab -e

Wenn vorher noch nie ein Cronjob ausgeführt wurde, wird beim erstmaligen Start abgefragt, welcher Editor zum Bearbeiten verwendet werden soll. Hier den eigenen, installierten Lieblingseditor auswählen, geeignet ist jeder.
Am Ende der Datei tragen wir dann folgende Zeile ein:
> 56 * * * * /vollständiger_Pfad/bin/S10auslesen

Der Pfad muss entsprechend angepasst werden und auf die eben kompilierte Datei verweisen. Die Zeichen vor dem Pfad bedeuten folgendes: der Befehl wird ausgeführt bei Minute 56 bei jeder Stunde * an jedem Tag im Monat * in jedem Monat * an beliebigen Wochentagen *

### S10 Hauskraftwerk konfigurieren

*S10auslesen* stellt eine Verbindung mit dem S10 Hauskraftwerk über das Modbus-Protokoll her. Dieses muss auf dem S10 Hauskraftwerk vorher aktiviert werden. Dazu gibt es ausführliche Anleitungen, z.&nbsp;B. das von E3/DC mitgelieferte und im [E3/DC online Portal](https://s10.e3dc.com/s10/) abrufbare Handbuch.
Alternativ existieren im Netz viele Anleitungen, z.&nbsp;B. als [Video](https://www.youtube.com/watch?v=a5RjLClphxA).
Die IP-Adresse des S10 Hauskraftwerks muss mit dem oben eingetragenen Wert in der Header-Datei übereinstimmen.

### SQL Datenbank einrichten

*S10auslesen* schreibt die Daten in eine MySQL-kompatible Datenbank. Die Einrichtung eines MySQL-Servers sprengt den Rahmen dieses Readmes, es gibt dazu aber viele ausführliche Anleitungen im Netz zu finden.
Als Beispiel dient [diese Anleitung](https://www.howtoforge.de/anleitung/so-installierst-du-mariadb-unter-debian-11/), welche der Autor verwendet hat für die Einrichtung seines eigenen SQL-Servers.

Es wird empfohlen, eine eigene Datenbank und dazugehörigen Benutzer für *S10auslesen* einzurichten. Das geschieht in der SQL-Konsole mittels den Befehlen:
> CREATE DATABASE S10HISTORIE;
> CREATE USER 'BENUTZER' IDENTIFIED BY 'PASSWORT';
> GRANT ALL PRIVILEGES ON S10HISTORIE.* TO BENUTZER;
> FLUSH RIVILEGES;

`BENUTZER`, `PASSWORT` und  `S10HISTORIE` müssen dabei angepasst werden und mit den oben eingetragenen Werten in der Header-Datei übereinstimmen.

*S10auslesen* schreibt eine Datenmenge von etwa 7&nbsp;MB pro Tag in die Datenbank, was pro Jahr ca. 2,5&nbsp;GB Speicherbelegung entspricht. Die verfügbare Festplattengröße sollte entsprechend ausreichend gewählt werden.

### JSON Dateiausgabe einrichten

*S10auslesen* gibt die eben ausgelesenen Messdaten des S10 Hauskraftwerks sekündlich als Datei im JSON-Format aus, welche für andere Anwendungen verwendet werden kann.
Der Pfad zur Ausgabedatei wurde mit dem oben eingetragenen Wert in der Header-Datei festgelegt.

*S10auslesen* überschreibt die Datei dabei einmal pro Sekunde. Vom Autor empfohlen wird daher, die Datei nicht auf die Festplatte zu schreiben, sondern z.&nbsp;B. in eine Ramdisk. Da die Datei selbst kleiner als 1&nbsp;kB ist, kann die Ramdisk auch entsprechend klein gewählt werden.
Auf einem Linux-Server kann die Ramdisk so eingerichtet werden, dass sie bei jedem Systemstart automatisch erzeugt wird. Dazu wird mit root-Rechten folgende Zeile am Ende der `/etc/fstab` eingetragen
> tmpfs /ram tmpfs defaults,size=16M 0 0

Der Ordner `/ram` muss dabei dem Pfad aus der Header-Datei oben entsprechen und muss bereits vor Einbinden der Ramdisk existieren und sollte leer sein, also keine Unterordner oder Dateien enthalten.
Wenn der Server nicht neu gestartet werden soll, reicht es, nach Abspeichern der `/etc/fstab` mit root-Rechten folgenden Kommandozeilenbefehl auszuführen:
> mount -a

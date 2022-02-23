/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * S10auslesen makes the current reading of a E3/DC S10 available for processing *
 * Copyright (C) 2018-2022 - senneschall <senneschall@web.de>                    *
 * This file is part of S10auslesen.                                             *
 *                                                                               *
 * S10auslesen is free software: you can redistribute it and/or modify           *
 * it under the terms of the GNU Affero General Public License as                *
 * published by the Free Software Foundation, either version 3 of the            *
 * License, or (at your option) any later version.                               *
 *                                                                               *
 * S10auslesen is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 *
 * GNU Affero General Public License for more details.                           *
 *                                                                               *
 * You should have received a copy of the GNU Affero General Public License      *
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#pragma once

/* Adresse des S10 Hauskraftwerks, welches ausgelesen werden soll */
#define S10_ADRESSE         "192.168.0.100"
/* Modbus-Port, auf dem die Verbindung aufgebaut wird */
#define S10_PORT            502

/* so viele Lesefehler werden toleriert bevor eine Neuverbindung aufgebaut wird */
#define LESEFEHLER_AKZEPT   60
/* Programmabbruch wenn nach so vielen Versuchen keine neue Verbindung aufgebaut werden konnte */
#define NEUVERBIND_AKZEPT   600

/* die Anzahl der Register mit den Identifikationsregistern, beginnend ab 0 */
#define IDENTIFIKREGISTER   67
/* die Anzahl der Register mit den Leistungswerten, beginnend ab IDENTIFIKREGISTER */
#define LEISTUNGSREGISTER   37

/* Adresse, unter der die SQL-Datenbank erreichbar ist */
#define SQL_ADRESSE         "localhost"
/* Benutzername des SQL-Users, der Schreibrechte auf der Datenbank hat */
#define SQL_USER            "BENUTZER"
/* zum Benutzer gehöriges Passwort */
#define SQL_PW              "PASSWORT"
/* Datenbankname, in welcher die Daten als neue Tabelle für jeden Tag eingetragen werden sollen */
#define SQL_DB              "S10HISTORIE"
/* SQL-Port, auf dem die Verbindung aufgebaut wird */
#define SQL_PORT            3306
/* (POSIX) SQL-Sockel über den die Verbindung aufgebaut wird */
#define SQL_SOCKET          "/run/mysqld/mysqld.sock"

/* Max-Länge des Tabellennamens worst case: 42 Zeichen */
#define LEN_TABELLENAME     48
/* Max-Länge des Tabellenkommentars worst case 238 Zeichen */
#define LEN_TABKOMMENTAR    256
/* Max-Länge des SQL-Querys für die Messwerte einer Sekunde worst case: 508 Zeichen */
#define LEN_WERTE           512
/* Max-Länge des SQL-Querys für die Erstellung der Tabelle worst case 1292 Zeichen */
#define LEN_TABELLE         1344

/* 60*60 Sekunden */
#define NO_DATEN            3600

/* Dateiname und Pfad, unter dem die JSON-Datei ausgegeben werden soll */
#define JSON_FILE           "/ram/s10daten.json"

/* 100ms Raster beim Warten auf die volle Stunde */
#define AUFLOESUNG_WART_NS  100000000
/* 30ms Raster beim sekundengenauen Abfragen der Daten */
#define AUFLOESUNG_LESE_NS  30000000

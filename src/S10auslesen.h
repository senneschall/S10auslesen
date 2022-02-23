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

#include <stdint.h> /* für int32_t und Konsorten */
#include <time.h> /* für struct tm */

/**
 * kapselt und sortiert die Leistungsdaten des S10, also die Register welche sich sekündlich ändern können.
 */
typedef struct
{
    int32_t const P_pv;
    int32_t const P_bat;
    int32_t const P_haus;
    int32_t const P_netz;
    int32_t const P_ext;
    int32_t const P_wall;
    int32_t const P_pvwall;
    uint8_t const eigen;
    uint8_t const autarkie;
    uint16_t const soc;
    uint16_t const notstr;
    uint16_t const ems;
    int16_t const emsrc;
    uint16_t const emsctrl;
    uint16_t const wall1;
    uint16_t const wall2;
    uint16_t const wall3;
    uint16_t const wall4;
    uint16_t const wall5;
    uint16_t const wall6;
    uint16_t const wall7;
    uint16_t const wall8;
    uint16_t const Vdc1;
    uint16_t const Vdc2;
    uint16_t const Vdc3;
    uint16_t const Idc1;
    uint16_t const Idc2;
    uint16_t const Idc3;
    uint16_t const Pdc1;
    uint16_t const Pdc2;
    uint16_t const Pdc3;
} s10daten;

/**
 * kapselt und sortiert die Werte der nicht-volatilen Register des S10, also die Daten welche sich nie oder nur selten ändern.
 */
typedef struct
{
    uint16_t const magic;
    uint8_t const mb_minor;
    uint8_t const mb_major;
    uint16_t const reg;
    char const hersteller[32];
    char const modell[32];
    char const seriennr[32];
    char const firmware[32];
} s10konstanten;

/**
 * liest die Identifikationsdaten des S10 einmalig aus.
 * @param s10konstanten_t welches mit den ausgelesenen Werten gefüllt wird.
 * @return EXIT_SUCCESS wenn die Daten erfolgreich gelesen werden konnten, sonst EXIT_FAILURE.
 */
int_fast8_t IdentifikationsblockAuslesenModbus(s10konstanten* const);

/**
 * schreibt die Identifikationsdaten des S10 in die Datums-abhängige SQL Datenbank.
 * @param s10konstanten_t mit den Identifikationsdaten, welche in die SQL Datenbank geschrieben werden sollen.
 * @param (tagesgenaues) Datum, für welches in der SQL Datenbank ggfs. eine neue Tabelle erzeugt wird.
 * @return Rückgabewert von mysql_query() falls SQL-Verbindung aufgebaut werden konnte, sonst EXIT_FAILURE.
 */
int_fast8_t IdentifikationsblockEintragenSQL(s10konstanten* const, struct tm const);

/**
 * liest die Leistungsdaten des S10 in der aktuellen Stunde sekundengenau aus.
 * @param Array s10daten_t, welches sekundengenau und zeitlich geordnet die ausgelesenen Messwerte beinhaltet.
 * @return EXIT_SUCCESS wenn die Daten erfolgreich gelesen werden konnten, sonst EXIT_FAILURE.
 */
int_fast8_t LeistungsdatenAuslesenModbus(s10daten* const);

/**
 * schreibt die  Leistungsdaten des S10 in die Datums-abhängige SQL Datenbank
 * @param Array s10daten_t, welches alle ausgelesenen Messwerte der letzten beinhaltet, die sekundengenau und zeitlich ansteigend geordnet sind.
 * @param (stundengenaues) Datum, für welche Stunde innerhalb der entspr. Einzeltages-Tabelle die Werte in die Datenbank eingetragen werden.
 * @return EXIT_SUCCESS wenn die Daten erfolgreich eingetragen werden konnten, sonst EXIT_FAILURE.
 */
int_fast8_t LeistungsdatenEintragenSQL(s10daten* const, struct tm const);

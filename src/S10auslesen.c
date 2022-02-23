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

#include "S10auslesen.h" /* zugehöriger Header dieser Datei */
#include "Einstellungen.h" /* für die Benutzerdaten */

#include <mariadb/mysql.h> /* für die SQL-Verbindung */
#include <modbus/modbus-tcp.h> /* für die Modbus-Verbindung */

#include <endian.h> /* Umwandlung E3/DC Big Endian zum Format des Host-Rechners */
#include <stdint.h> /* für int32_t und Konsorten */
#include <stdio.h> /* für Dateioperation und String-Formatierung */
#include <stdlib.h> /* Speicherbehandlung und Rückgabewerte */
#include <string.h> /* String-Operationen */
#include <time.h> /* für die Funktionen rund um die Datums- und Zeitberechnungen */

/**
 * gibt die aktuelle - sprich im Moment der Codeausführung vorliegenden - Datum und Zeit zurück.
 * @return gibt Datum und Uhrzeit des aktuellen Zeitpunkts zurück.
 */
static inline struct tm Jetzt(void)
{
    time_t const uhrzeit = time(NULL);
    return *gmtime(&uhrzeit);
}

/**
 * Datum und Zeit (auf die Stunde genau), für welches das die Leistungsdaten des S10 ausgelesen werden.
 * @return das (stundengenaue) Datum und Uhrzeit, für welches die Leistungsdaten ausgelesen werden.
 */
static inline struct tm StartDatum(void)
{
    struct tm jetzt = Jetzt();
    jetzt.tm_hour++;
    /* normalisieren (z.b. aus dem 32.Januar muss der 1.Februar gemacht werden
	 * Schritt 1: Umwandeln von dd.mm.yyyy -> arithmetisches time_t (= 64bit mit Sekunden seit 1.1.1970) */
    time_t const start = mktime(&jetzt);
    /* 2. Schritt: aus dem generischen uhrzeit wird wieder ein Datum ---> zeit enthält das Datum der nächsten Stunde! */
    return *gmtime(&start);
}

/**
 * legt das Programm schlafen bis die volle Stunde erreicht wird, also bis Minute = 0 und Sekunde = 0 erreicht wird.
 */
static inline void SchlafeBisVolleStunde(void)
{
    struct timespec const feinSchlafen = { 0, AUFLOESUNG_WART_NS };
    int_fast8_t schlafabbruch = 0;
    struct tm jetzt;

    do
    {
        jetzt = Jetzt();
        time_t const wartezeit = 60 * (59 - jetzt.tm_min) + 58 - jetzt.tm_sec; /* Sekunden bis hh:59:58 Uhr */
        struct timespec const grobSchlafen = { wartezeit < 0 ? 0 : wartezeit, 0 };
        schlafabbruch = nanosleep(&grobSchlafen, NULL);
    } while (schlafabbruch);
    /* es ist jetzt HH:59:58 (2 Sekunden Abstand zur Uhrzeit weil die Nachkommastelle nicht bekannt ist
	 * falls der nächste Schritt die nächste Sekunde voll macht würde es einen Zeitverzug von einer Stunde ergeben) */

    do
    {
        nanosleep(&feinSchlafen, NULL);
        jetzt = Jetzt();
    } while (jetzt.tm_sec > 0 && jetzt.tm_min > 0);
}

/**
 * Speichert die Leistungsdaten als Datei im JSON-Format.
 * @param p die Leistungsdaten der aktuellen Sekunde, welche als JSON-Datei ausgegeben werden sollen.
 */
static inline void ErzeugeJSON(s10daten *const p)
{
    FILE *const f = fopen(JSON_FILE, "w");
    if (NULL != f)
    {
        fprintf(f,
                "{\"Ppv\":%d,\"Pbat\":%d,\"Phaus\":%d,\"Pnetz\":%d,\"Pext\":%d,\"Pwbx\":%d,\"Ppvwb\":%d,\"eigen\":%hhu,\"autark\":%hhu,\"SOC\":%hu,"
				"\"NOT\":%hu,\"EMS\":%hu,\"Wb1\":%hu,\"Wb2\":%hu,\"Wb3\":%hu,\"Wb4\":%hu,\"Wb5\":%hu,\"Wb6\":%hu,\"Wb7\":%hu,\"Wb8\":%hu,\"Vdc1\":%hu,"
				"\"Vdc2\":%hu,\"Vdc3\":%hu,\"Idc1\":%hu,\"Idc2\":%hu,\"Idc3\":%hu,\"Pdc1\":%hu,\"Pdc2\":%hu,\"Pdc3\":%hu}",
                p->P_pv, p->P_bat, p->P_haus, p->P_netz, p->P_ext, p->P_wall, p->P_pvwall, p->eigen, p->autarkie, p->soc, p->notstr, p->ems, p->wall1,
                p->wall2, p->wall3, p->wall4, p->wall5, p->wall6, p->wall7, p->wall8, p->Vdc1, p->Vdc2, p->Vdc3, p->Idc1, p->Idc2, p->Idc3, p->Pdc1,
                p->Pdc2, p->Pdc3);
        fclose(f);
    }
}

/**
 * liest die Leistungsdaten des S10 in der aktuellen Stunde sekundengenau aus.
 * 1) Stellt eine Modbus-Verbindung mit dem S10-Hauskraftwerk her
 * 2) liest die Leistungsdaten mit den sekundengenauen Messwerten des S10 aus und bereitet sie maschinenlesbar auf.
 * 3) steuert das Aufrufen von Funktionen, welche die Daten sekundengenau als Datei ausgeben.
 * 4) stellt bei Verbindungsverlust eine neue Verbindung her während die zeitliche Reihenfolge der Daten aufrechterhalten wird
 *    indem Nulleinträge für jede Sekunde ohne auslesbare Daten erzeugt werden.
 * @param aktstundenmesswerte Array, in das für die aktuelle Stunde für jede Sekunde die ausgelesenen Leistungsdaten steigend angeordnet eingetragen wird.
 * @return EXIT_SUCCESS wenn die Daten erfolgreich gelesen werden konnten, sonst EXIT_FAILURE.
 */
int_fast8_t LeistungsdatenAuslesenModbus(s10daten *const aktstundenmesswerte)
{
    int_fast8_t result = EXIT_FAILURE;

    modbus_t *modbus = modbus_new_tcp(S10_ADRESSE, S10_PORT);
    if (NULL == modbus) return result;
    if (0 == modbus_connect(modbus))
    {
        uint16_t *const modbuslesewert = (uint16_t*) calloc(LEISTUNGSREGISTER, sizeof(uint16_t));
        if (NULL != modbuslesewert)
        {
            struct timespec zeit;
            if (0 == clock_gettime(CLOCK_MONOTONIC_COARSE, &zeit))
            {
                struct timespec const feinSchlafen = { 0, AUFLOESUNG_LESE_NS };
                struct timespec const grobSchlafen = { 1, 0 };
                time_t const startsek = zeit.tv_sec;
                size_t sekunden = 0;
                int_fast16_t fehler = 0;
                while (sekunden < NO_DATEN)
                {
                    clock_gettime(CLOCK_MONOTONIC_COARSE, &zeit); /* kein neuer Check notwendig da durch folgende if-Schleife indirekt abgedeckt */
                    if (zeit.tv_sec > startsek + sekunden)
                    {
                        if (LEISTUNGSREGISTER == modbus_read_registers(modbus, IDENTIFIKREGISTER, LEISTUNGSREGISTER, modbuslesewert))
                        {
                            memcpy(aktstundenmesswerte + sekunden, modbuslesewert, LEISTUNGSREGISTER * sizeof(uint16_t));
                            ErzeugeJSON(aktstundenmesswerte + sekunden);
                        }
                        else /* Fehlerfall modbus_read_registers */
                        {
                            fehler++;
                            if (fehler > LESEFEHLER_AKZEPT)
                            {
                                modbus_close(modbus);
                                modbus_free(modbus);

                                int_fast16_t neuverbindzahl = 0; /* # Neuverbindungsversuche (1 pro Sekunde) */
                                do
                                {
                                    if (0 == nanosleep(&grobSchlafen, NULL)) sekunden++;
                                    modbus = modbus_new_tcp(S10_ADRESSE, S10_PORT);
                                    neuverbindzahl++;
                                    if (neuverbindzahl > NEUVERBIND_AKZEPT) goto verbindungverloren;
                                } while (NULL == modbus);

                                neuverbindzahl = 0;
                                do
                                {
                                    if (0 == nanosleep(&grobSchlafen, NULL)) sekunden++;
                                    neuverbindzahl++;
                                    if (neuverbindzahl > NEUVERBIND_AKZEPT) goto verbindungverloren;
                                } while (0 != modbus_connect(modbus));

                                fehler = 0; /* nach erfolgreichem Neuverbinden wird der Fehlerzähler zurückgesetzt */
                            }
                        }
                        sekunden++;
                    }
                    else
                        nanosleep(&feinSchlafen, NULL);
                }
                result = EXIT_SUCCESS;
            }
        }
verbindungverloren:
        free(modbuslesewert);
    }
    modbus_close(modbus);
    modbus_free(modbus);
    return result;
}

/**
 * schreibt die  Leistungsdaten des S10 in die Datums-abhängige SQL Datenbank
 * 1) stellt eine Verbindung zur MySQL-Datenbank her.
 * 2) schreibt alle Werte der mit dem Array s10daten_t bereitgestellten Leistungsdaten in die Datenbank.
 * 3) überprüft die Datenbank auf Nulleinträge (welche bei Verbindungsabbrüchen entstehen) und löscht diese aus der Datenbank.
 * @param daten Array, umfasst für die aktuelle Stunde für jede Sekunde die ausgelesenen Leistungsdaten sekundengenau
 *        und zeitlich ansteigend geordnet in s10daten_t.
 * @param zeit (stundengenaues) Datum, für welche Stunde innerhalb der entspr. Einzeltages-Tabelle die ausgelesenen Leistungsdaten
 *        in die Datenbank eingetragen werden.
 * @return EXIT_SUCCESS wenn die Daten erfolgreich eingetragen werden konnten, sonst EXIT_FAILURE.
 */
int_fast8_t LeistungsdatenEintragenSQL(s10daten *const daten, struct tm const zeit)
{
    int_fast8_t result = EXIT_FAILURE;
    MYSQL *const sqlconnection = mysql_init(NULL);
    if (NULL != sqlconnection)
    {
        if (NULL != mysql_real_connect(sqlconnection, SQL_ADRESSE, SQL_USER, SQL_PW, SQL_DB, SQL_PORT, SQL_SOCKET, 0))
        {
            char *const tabellenname = (char*) malloc(LEN_TABELLENAME * sizeof(char));
            if (NULL != tabellenname)
            {
                sprintf(tabellenname, "%04d_%02d_%02d", zeit.tm_year + 1900, zeit.tm_mon + 1, zeit.tm_mday);
                char *const zeitpunkt = (char*) malloc(LEN_TABELLENAME * sizeof(char));
                if (NULL != zeitpunkt)
                {
                    char *const valstring = (char*) malloc(LEN_WERTE * sizeof(char));
                    if (NULL != valstring)
                    {
                        char *const sqlstring = (char*) malloc(LEN_WERTE * sizeof(char));
                        if (NULL != sqlstring)
                        {
                            for (size_t cnt = 0; cnt < NO_DATEN; cnt++)
                            {
                                /* wenn alle Werte gleich 0 sind, sind das noch die calloc-Init-Werte --> überspringe diesen Datenpunkt */
                                if (0 == daten[cnt].P_pv && 0 == daten[cnt].P_bat && 0 == daten[cnt].P_haus && 0 == daten[cnt].P_netz
                                    && 0 == daten[cnt].P_ext && 0 == daten[cnt].P_wall && 0 == daten[cnt].P_pvwall && 0 == daten[cnt].eigen
                                    && 0 == daten[cnt].autarkie && 0 == daten[cnt].soc && 0 == daten[cnt].notstr && 0 == daten[cnt].ems
                                    && 0 == daten[cnt].wall1 && 0 == daten[cnt].wall2 && 0 == daten[cnt].wall3 && 0 == daten[cnt].wall4
                                    && 0 == daten[cnt].wall5 && 0 == daten[cnt].wall6 && 0 == daten[cnt].wall7 && 0 == daten[cnt].wall8
                                    && 0 == daten[cnt].Vdc1 && 0 == daten[cnt].Vdc2 && 0 == daten[cnt].Vdc3 && 0 == daten[cnt].Idc1
                                    && 0 == daten[cnt].Idc2 && 0 == daten[cnt].Idc3 && 0 == daten[cnt].Pdc1 && 0 == daten[cnt].Pdc2
                                    && 0 == daten[cnt].Pdc3) continue;

                                sqlstring[0] = '\0'; /* neuen Stringinhalt anlegen */
                                strcat(sqlstring, "INSERT ");
                                strcat(sqlstring, tabellenname);
                                sprintf(zeitpunkt, "%02d:%02zu:%02zu", zeit.tm_hour, (cnt - (cnt % 60)) / 60, cnt % 60);
                                sprintf(valstring, " VALUES('%s',"
                                        "%d,%d,%d,%d,%d,%d,%d,%hhu,%hhu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu,%hu);",
                                        zeitpunkt, daten[cnt].P_pv, daten[cnt].P_bat, daten[cnt].P_haus, daten[cnt].P_netz, daten[cnt].P_ext,
                                        daten[cnt].P_wall, daten[cnt].P_pvwall, daten[cnt].eigen, daten[cnt].autarkie, daten[cnt].soc,
                                        daten[cnt].notstr, daten[cnt].ems, daten[cnt].wall1, daten[cnt].wall2, daten[cnt].wall3, daten[cnt].wall4,
                                        daten[cnt].wall5, daten[cnt].wall6, daten[cnt].wall7, daten[cnt].wall8, daten[cnt].Vdc1, daten[cnt].Vdc2,
                                        daten[cnt].Vdc3, daten[cnt].Idc1, daten[cnt].Idc2, daten[cnt].Idc3, daten[cnt].Pdc1, daten[cnt].Pdc2,
                                        daten[cnt].Pdc3);
                                strcat(sqlstring, valstring);
                                mysql_query(sqlconnection, sqlstring);
								/* bei Fehler (z.B. doppelter Zeitwert kann nicht eingefügt werden), gehe einfach zum nächsten Wert über */
                            }

                            result = EXIT_SUCCESS;

                            sqlstring[0] = '\0'; /* neuen Stringinhalt anlegen */
                            /* Datenbankpflege -> lösche die Nulleinträge die sich trotzdem eingeschlichen haben */
                            strcat(sqlstring, "DELETE FROM ");
                            strcat(sqlstring, tabellenname);
                            strcat(sqlstring,
                                   " WHERE Ppv=0 AND Pbat=0 AND Phaus=0 AND Pnetz=0 AND Pext=0 AND Pwall=0 AND Ppvwall=0 AND eigen=0 AND autarkie=0 AND soc=0"
								   " AND notstrom=0 AND status=0 AND wall1=0 AND wall2=0 AND wall3=0 AND wall4=0 AND wall5=0 AND wall6=0 AND wall7=0"
								   " AND wall8=0 AND Vdc1=0 AND Vdc2=0 AND Vdc3=0 AND Idc1=0 AND Idc2=0 AND Idc3=0 AND Pdc1=0 AND Pdc2=0 AND Pdc3=0;");
                            mysql_query(sqlconnection, sqlstring);

                            free(sqlstring);
                        }
                        free(valstring);
                    }
                    free(zeitpunkt);
                }
                free(tabellenname);
            }
        }
        mysql_close(sqlconnection);
    }
    return result;
}

/**
 * 1) Stellt eine Modbus-Verbindung mit dem S10-Hauskraftwerk her
 * 2) liest die Identifikationsdaten (nicht-volatile Register) des S10 aus und bereitet sie maschinenlesbar auf.
 * @param konstanten s10konstanten_t, in welches die Identifikationsdaten eingetragen werden.
 * @return EXIT_SUCCESS wenn die Daten erfolgreich gelesen werden konnten, sonst EXIT_FAILURE.
 */
int_fast8_t IdentifikationsblockAuslesenModbus(s10konstanten *const konstanten)
{
    int_fast8_t result = EXIT_FAILURE;
    modbus_t *const modbus = modbus_new_tcp(S10_ADRESSE, S10_PORT);
    if (NULL != modbus)
    {
        if (0 == modbus_connect(modbus))
        {
            uint16_t *const rohwerte = (uint16_t*) calloc(IDENTIFIKREGISTER, sizeof(uint16_t));
            if (NULL != rohwerte)
            {
                if (IDENTIFIKREGISTER == modbus_read_registers(modbus, 0, IDENTIFIKREGISTER, rohwerte))
                {
                    /* Endianness für Strings korrigieren */
                    for (int_fast8_t i = 3; i < IDENTIFIKREGISTER; i++)
                        rohwerte[i] = be16toh(rohwerte[i]);

                    /* umständliches Casting kann vermieden werden weil die Bitreihenfolge in rohwerte korrekt ist und auf konstanten kopiert werden kann */
                    memcpy(konstanten, rohwerte, IDENTIFIKREGISTER * sizeof(uint16_t));
                    result = EXIT_SUCCESS;
                }
                free(rohwerte);
            }

            modbus_close(modbus);
            modbus_free(modbus);
        }
    }
    return result;
}

/**
 * 1) stellt eine Verbindung zur MySQL-Datenbank her.
 * 2) überprüft ob für das aktuelle Datum eine Tabelle existiert und legt diese ggfs. neu an
 * 3) schreibt die Identifikationsdaten aus s10konstanten_t in die Datenbank.
 * @param konstanten s10konstanten_t mit den Identifikationsdaten, welche in die SQL Datenbank geschrieben werden sollen.
 * @param startdatum (tagesgenaues) Datum, für welches in der SQL Datenbank ggfs. eine neue Tabelle erzeugt wird.
 * @return Rückgabewert von mysql_query() falls SQL-Verbindung aufgebaut werden konnte, sonst EXIT_FAILURE.
 */
int_fast8_t IdentifikationsblockEintragenSQL(s10konstanten *const konstanten, struct tm const startdatum)
{
    int_fast8_t result = EXIT_FAILURE;

    MYSQL *const sqlconnection = mysql_init(NULL);
    if (NULL == sqlconnection) return result;

    if (NULL != mysql_real_connect(sqlconnection, SQL_ADRESSE, SQL_USER, SQL_PW, SQL_DB, SQL_PORT, SQL_SOCKET, 0))
    {
        char *const tabellenname = (char*) malloc(LEN_TABELLENAME * sizeof(char));
        if (NULL != tabellenname)
        {
            char *const tabellebkommentar = (char*) malloc(LEN_TABKOMMENTAR * sizeof(char));
            if (NULL != tabellebkommentar)
            {
                char *const sqlquery = (char*) malloc(LEN_TABELLE * sizeof(char));
                if (NULL != sqlquery)
                {
                    /* wird nur einmal zu Beginn aufgerufen, hier ist Lesbarkeit und Wartbarkeit wichtiger als Effizienz -> separate strcat */
                    strcpy(sqlquery, "CREATE TABLE IF NOT EXISTS ");
                    sprintf(tabellenname, "%04d_%02d_%02d", startdatum.tm_year + 1900, startdatum.tm_mon + 1, startdatum.tm_mday);
                    strcat(sqlquery, tabellenname);
                    strcat(sqlquery, " (uhrzeit TIME PRIMARY KEY");
                    strcat(sqlquery, ",Ppv SMALLINT SIGNED NOT NULL");
                    strcat(sqlquery, ",Pbat SMALLINT SIGNED NOT NULL");
                    strcat(sqlquery, ",Phaus SMALLINT SIGNED NOT NULL");
                    strcat(sqlquery, ",Pnetz SMALLINT SIGNED NOT NULL");
                    strcat(sqlquery, ",Pext SMALLINT SIGNED NOT NULL");
                    strcat(sqlquery, ",Pwall SMALLINT SIGNED NOT NULL");
                    strcat(sqlquery, ",Ppvwall SMALLINT SIGNED NOT NULL");
                    strcat(sqlquery, ",eigen TINYINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",autarkie TINYINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",soc TINYINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",notstrom TINYINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",status TINYINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",wall1 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",wall2 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",wall3 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",wall4 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",wall5 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",wall6 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",wall7 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",wall8 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",Vdc1 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",Vdc2 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",Vdc3 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",Idc1 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",Idc2 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",Idc3 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",Pdc1 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",Pdc2 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ",Pdc3 SMALLINT UNSIGNED NOT NULL");
                    strcat(sqlquery, ")MAX_ROWS=86400 COMMENT='");
                    sprintf(tabellebkommentar, "%#X Modbus: %u.%u Register: %u Hersteller: %s Modell: %s No: %s Firmware: %s';", konstanten->magic,
                            konstanten->mb_major, konstanten->mb_minor, konstanten->reg, konstanten->hersteller, konstanten->modell,
                            konstanten->seriennr, konstanten->firmware);
                    strcat(sqlquery, tabellebkommentar);

                    result = mysql_query(sqlconnection, sqlquery);
                    free(sqlquery);
                }
                free(tabellebkommentar);
            }
            free(tabellenname);
            mysql_close(sqlconnection);
        }
    }
    return result;
}

/**
 * 1) stellt sofort bei Aufrug eine Verbindung zum S10 Hauskraftwerk her, liest die Identifikationsdaten aus und trägt sie in datumsabhängige SQL-Tabellen ein
 * 2) wartet bis die nächste Stunde anbricht, und liest während der kommenden Stunde die Leistungswerte aus dem S10 aus, stellt sie sekundengenau
 *    als Dateiausgabe bereit und trägt sie nach Vollendung der Stunde in die Datenbank ein.
 * @return EXIT_SUCCESS wenn alle Programmschritt erfolgreich durchgeführt werden konnten, sonst EXIT_FAILURE.
 */
int main(void)
{
    struct tm const startdatum = StartDatum();

    s10konstanten *const id = (s10konstanten*) calloc(1, sizeof(s10konstanten));
    if (NULL == id) return EXIT_FAILURE;
    /* Das Auslesen und Eintragen des ID-Blocks dient gleichzeitig als Verbindungstest zum S10 und SQL-Server */
    if (IdentifikationsblockAuslesenModbus(id)) goto idfehler;
    if (IdentifikationsblockEintragenSQL(id, startdatum)) goto idfehler;
    free(id);

    s10daten *const daten = (s10daten*) calloc(NO_DATEN, sizeof(s10daten));
    if (NULL == daten) return EXIT_FAILURE;
    SchlafeBisVolleStunde();
    if (LeistungsdatenAuslesenModbus(daten)) goto datfehler;
    if (LeistungsdatenEintragenSQL(daten, startdatum)) goto datfehler;
    free(daten);

    return EXIT_SUCCESS;

idfehler:
    free(id);
    return EXIT_FAILURE;
datfehler:
    free(daten);
    return EXIT_FAILURE;
}

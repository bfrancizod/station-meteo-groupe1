// Rôle : Coordonne les modes, les commandes, les acquisitions et la signalisation de la station.
// Chef d'orchestre de la station : modes, commandes, acquisition et signalisation.
// Les services de services.h relient cette logique aux pilotes, à la RTC et à la SD.
// Les mesures passent ensuite par format.c vers le fichier ou la console de maintenance.
/* Supervision coopérative : aucun délai de plusieurs minutes.
 * Les services matériels sont courts ; la SD appelle App_Background pendant
 * ses attentes pour garder boutons, GPS et signalisation disponibles. */
#include "board.h"
#include "grove_rgb.h"
#include "services.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VERSION "WWW-G1-1.1.0"
#define LOT "CESI-G1-2026"
// Délai du GPS en soutenance, en millisecondes, indépendant du TIMEOUT des autres capteurs.
#define GPS_DEMO_TIMEOUT_MS 15000u
// static au niveau du fichier rend ces variables privées à app.c ; elles persistent
// entre les appels. Sans initialiseur explicite, elles démarrent à zéro.
static Config cfg;
static AppMode mode, previous_mode;
// Échéances et instants en millisecondes ; env_due et light_due permettent de revenir
// dans la boucle entre deux tentatives sans attendre l'expiration du délai maximal.
static uint32_t next_cycle, last_activity, cycle_start, env_due, light_due;
// Masques de défauts : chaque bit représente une cause, plusieurs peuvent coexister.
static uint32_t errors, sd_error;
// cycle : collecte en cours ; env_ok : pilote initialisé ; env_pending : conversion lancée.
static bool initialized, cycle, env_ok, env_pending, request_gps, force_measure;
// Option temporaire de soutenance : jamais enregistrée dans la configuration Flash.
// Elle est activée par GPS_DEMO=1 et revient à faux à chaque démarrage dans App_Init.
static bool gps_demo;
static unsigned economic_cycle;
static Measurement measurement;
// Un compteur et deux bits de diagnostic par capteur évitent qu'un capteur rétabli
// efface l'erreur d'un autre. Deux cycles dépassant le délai d'attente déclenchent l'erreur d'accès.
static uint8_t timeout_count[SENSOR_COUNT];
static uint8_t sensor_access, sensor_invalid;
// Tampon de commande : un tableau de caractères terminé par '\0' avant son analyse.
// command_used compte les caractères utiles ; une place est réservée au terminateur.
static char command_line[96];
static unsigned command_used;
static bool command_overflow;
static bool last_was_cr;
// raw = lecture instantanée, stable = état après filtrage, sent = appui déjà signalé.
// changed date le dernier rebond ; pressed date le début de l'appui stable.
typedef struct {
    bool raw, stable, sent;
    uint32_t changed, pressed;
} Button;
static Button red, green;
// Les événements sont produits par App_Background et consommés par App_Process.
// Ces deux fonctions s'exécutent dans le même flot principal, pas dans une IRQ.
static uint8_t events;
// Enfile une chaîne terminée par zéro pour l'UART ; ne garantit pas sa réception par le PC.
// const interdit de modifier les caractères à travers le pointeur s.
static void output(const char *s) {
    Board_ConsoleWrite(s);
}
// Renvoie une chaîne constante correspondant à l'énumération AppMode courante.
static const char *mode_name(void) {
    static const char *names[] = {"STANDARD", "CONFIGURATION", "MAINTENANCE", "ECONOMIQUE"};
    return names[mode];
}
// Convertit l'intervalle configuré en minutes vers des millisecondes, doublé en économie.
static uint32_t period(void) {
    return (uint32_t)cfg.interval_min * 60000u * (mode == MODE_ECONOMIC ? 2u : 1u);
}
// Met à jour le bouton pointé par b ; raw est vrai si pressé, now est en ms.
// b->champ accède à la structure désignée par le pointeur, sans en faire une copie.
// Un appui doit rester stable 30 ms, puis durer 5 s ; il faut relâcher pour réarmer.
static void button_process(Button *b, bool raw, uint32_t now, uint8_t event) {
    if (raw != b->raw) {
        b->raw = raw;
        b->changed = now;
    }
    // La soustraction non signée reste utilisable lorsque le compteur 32 bits reboucle,
    // à condition de ne pas laisser passer un tour complet entre les observations.
    if (raw != b->stable && now - b->changed >= 30) {
        b->stable = raw;
        if (raw)
            b->pressed = now;
        else
            b->sent = false;
    }
    if (b->stable && !b->sent && now - b->pressed >= 5000) {
        // |= ajoute le bit de cet événement sans effacer un éventuel autre événement.
        events |= event;
        b->sent = true;
    }
}
// Actualise la LED depuis la boucle : un défaut masque la couleur du mode sans changer le mode.
// now % 1000 donne la phase d'un cycle de 1 s ; 333/667 ms approche le rapport 1:2 demandé.
static void led(uint32_t now) {
    uint8_t r = 0, g = 0, b = 0;
    uint32_t phase = now % 1000;
    if (errors) {
        // Si plusieurs erreurs sont actives, cet ordre choisit celle montrée par la LED.
        // & teste la présence d'un bit ; le GPS correspond au dernier cas de cette sélection.
        unsigned code = (errors & ERR_RTC)              ? 0
                        : (errors & ERR_SD_IO)          ? 1
                        : (errors & ERR_SD_FULL)        ? 2
                        : (errors & ERR_SENSOR_ACCESS)  ? 3
                        : (errors & ERR_SENSOR_INVALID) ? 4
                                                        : 5;
        unsigned red_time = (code == 1 || code == 4) ? 333 : 500;
        if (phase < red_time)
            r = 50;
        else
            switch (code) {
            case 0:
                b = 50;
                break;
            case 1:
            case 2:
                r = g = b = 50;
                break;
            case 3:
            case 4:
                g = 50;
                break;
            default:
                r = g = 50;
                break;
            }
    } else
        switch (mode) {
        case MODE_STANDARD:
            g = 50;
            break;
        case MODE_CONFIGURATION:
            r = g = 50;
            break;
        case MODE_ECONOMIC:
            b = 50;
            break;
        case MODE_MAINTENANCE:
            r = 50;
            g = 15;
            break;
        }
    // old conserve la dernière couleur entre les appels ; l'envoi P9813 n'a lieu
    // qu'au changement. << décale les composantes et | les regroupe dans un entier.
    static uint32_t old = 0xffffffff;
    uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    if (color != old) {
        GroveRGB_SetColor(r, g, b);
        old = color;
    }
}
// Service coopératif rappelé aussi pendant les attentes SD via Board_Pump.
// Il décode le GPS, filtre les boutons et anime la LED ; les IRQ UART transportent
// seulement les octets. L'exécution des commandes reste dans console_process.
void App_Background(void) {
    if (!initialized)
        return;
    uint32_t now = HAL_GetTick();
    Gps_Process();
    button_process(&red, Board_Red(), now, 1);
    button_process(&green, Board_Green(), now, 2);
    led(now);
    /* Les commandes sont exécutées dans App_Process, jamais dans un appel FatFs. */
}
/* Une transition annule la mesure non publiée et libère la SD dans la boucle, jamais en
 * interruption. */
// target est le nouveau mode et now l'instant de transition en ms ; aucun résultat renvoyé.
// Annule la collecte courante, réarme l'ordonnancement et met à jour le montage SD.
// Les événements reçus pendant une écriture sont traités après son retour : la libération
// ne coupe donc pas un appel FatFs en cours. Les fichiers sont fermés après chaque ligne.
static void set_mode(AppMode target, uint32_t now) {
    if (mode == target)
        return;
    cycle = false;
    force_measure = false;
    if (target == MODE_MAINTENANCE) {
        previous_mode = mode;
        Storage_Release();
        output("SD liberee : retrait autorise.\r\n");
        sd_error = 0;
    }
    if (mode == MODE_MAINTENANCE) {
        sd_error = Storage_Mount() ? 0 : ERR_SD_IO;
        output(sd_error ? "ERREUR remontage SD\r\n" : "SD remontee\r\n");
    }
    mode = target;
    economic_cycle = 0;
    next_cycle = now;
    last_activity = now;
    char s[64];
    snprintf(s, sizeof(s), "MODE %s\r\n", mode_name());
    output(s);
}
// Affiche la configuration actuellement utilisée en RAM, éventuellement chargée de la Flash.
// Les valeurs ne sont donc pas forcément les valeurs par défaut de Config_Defaults.
static void show_config(void) {
    char s[512];
    snprintf(
        s, sizeof(s),
        "LOG_INTERVAL=%u\r\nFILE_MAX_SIZE=%lu\r\nTIMEOUT=%u\r\nLUMIN=%u\r\nLUMIN_LOW=%u\r\nLUMIN_"
        "HIGH=%u\r\nTEMP_AIR=%u\r\nMIN_TEMP_AIR=%.2f\r\nMAX_TEMP_AIR=%.2f\r\nHYGR=%u\r\nHYGR_MINT=%"
        ".2f\r\nHYGR_MAXT=%.2f\r\nPRESSURE=%u\r\nPRESSURE_MIN=%.2f\r\nPRESSURE_MAX=%.2f\r\n",
        cfg.interval_min, (unsigned long)cfg.file_max, cfg.timeout_s, cfg.lumin, cfg.lumin_low,
        cfg.lumin_high, cfg.temp, (double)cfg.temp_min, (double)cfg.temp_max, cfg.hygr,
        (double)cfg.hygr_mint, (double)cfg.hygr_maxt, cfg.pressure, (double)cfg.pressure_min,
        (double)cfg.pressure_max);
    output(s);
}
/* Modifier une composante après validation ; une RTC absente ne crée pas de date automatique. */
// Reçoit le nom et la valeur d'une commande RTC, renvoie vrai si l'écriture réussit.
// draft garde un brouillon entre commandes quand la RTC est invalide ; une DATE explicite
// reste nécessaire. &d transmet l'adresse de d pour que Time_Read puisse la remplir.
static bool rtc_command(const char *key, const char *value) {
    DateTime d;
    bool valid = Time_Read(&d);
    static DateTime draft = {2000, 1, 1, 0, 0, 0, 7};
    if (!valid)
        d = draft;
    unsigned a, b, c;
    char extra;
    if (!strcmp(key, "CLOCK")) {
        // Le quatrième champ %c détecte un caractère en trop après les trois nombres.
        if (sscanf(value, "%u:%u:%u%c", &a, &b, &c, &extra) != 3 || a > 23 || b > 59 || c > 59)
            return false;
        d.hour = a;
        d.minute = b;
        d.second = c;
    } else if (!strcmp(key, "DATE")) {
        if (sscanf(value, "%u,%u,%u%c", &a, &b, &c, &extra) != 3 || a < 1 || a > 12 || b < 1 ||
            b > 31 || c < 2000 || c > 2099)
            return false;
        d.month = a;
        d.day = b;
        d.year = c;
        if (!Time_Valid(&d))
            return false;
        d.weekday = Time_Weekday(&d);
    } else if (!strcmp(key, "DAY")) {
        static const char *names[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        unsigned i;
        for (i = 0; i < 7 && strcmp(names[i], value); i++) {
        }
        if (i == 7)
            return false;
        d.weekday = i + 1;
    } else
        return false;
    if (!Time_Valid(&d))
        return false;
    /* Si RTC invalide, CLOCK seul ne publie pas une fausse date par défaut.
     * DATE doit explicitement définir la date avant la première écriture. */
    if (!valid && strcmp(key, "DATE")) {
        draft = d;
        output("RTC non reglee : saisir DATE=MM,JJ,AAAA puis CLOCK=HH:MM:SS\r\n");
        return false;
    }
    draft = d;
    if (!Time_Write(&d))
        return false;
    // ~ inverse le masque ; &= retire seulement ERR_RTC en préservant les autres défauts.
    errors &= ~ERR_RTC;
    return true;
}
/* Analyser une ligne complète ; appliquer les réglages après leur sauvegarde effective. */
// Appelée par la console dans la boucle principale, jamais pendant un accès SD imbriqué.
// line est modifiable : le '=' sera remplacé par zéro pour séparer le nom et la valeur.
// Les commandes de consultation précèdent le contrôle du mode ; les réglages le suivent.
static void execute(char *line) {
    if (!*line)
        return;
    if (!strcmp(line, "HELP")) {
        output("HELP STATUS SHOW VERSION I2C MEASURE GPS_DEMO=0/1\r\nConfiguration : PARAM=valeur RESET "
               "CLOCK=HH:MM:SS DATE=MM,JJ,AAAA DAY=MON\r\n");
        return;
    }
    if (!strcmp(line, "VERSION")) {
        output(VERSION " LOT=" LOT "\r\n");
        return;
    }
    if (!strcmp(line, "SHOW")) {
        show_config();
        return;
    }
    if (!strcmp(line, "STATUS")) {
        char s[256];
        DateTime d;
        bool rtc = Time_Read(&d);
        if (rtc)
            errors &= ~ERR_RTC;
        else
            errors |= ERR_RTC;
        snprintf(
            s, sizeof(s),
            "MODE=%s ERR=0x%02lX RAW_PA10=%u RAW_PB3=%u RED=%u GREEN=%u RX_TX_LOST=%lu RTC=%s GPS_DEMO=%u\r\n",
            mode_name(), (unsigned long)errors, HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10),
            HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3), Board_Red(), Board_Green(),
            (unsigned long)Board_RxOverflows(), rtc ? "OK" : "INVALIDE/ABSENTE", (unsigned)gps_demo);
        output(s);
        if (rtc) {
            snprintf(s, sizeof(s), "RTC=%04u-%02u-%02u %02u:%02u:%02u\r\n", d.year, d.month, d.day,
                     d.hour, d.minute, d.second);
            output(s);
        }
        return;
    }
    if (!strcmp(line, "I2C")) {
        // Le scan vérifie une réponse à une adresse, pas la validité des mesures ni les tensions.
        // La HAL attend l'adresse 7 bits décalée d'un bit ; chaque essai est limité à 3 ms.
        output("I2C presents :");
        for (unsigned a = 1; a < 127; a++) {
            if (HAL_I2C_IsDeviceReady(&hi2c1, a << 1, 1, 3) == HAL_OK) {
                char s[8];
                snprintf(s, sizeof(s), " %02X", a);
                output(s);
            }
            App_Background();
        }
        output("\r\nAttendus : 68 et 76/77\r\n");
        return;
    }
    if (!strcmp(line, "MEASURE")) {
        if (mode == MODE_CONFIGURATION || cycle)
            output("REFUS : configuration ou acquisition en cours\r\n");
        else {
            force_measure = true;
            output("Mesure de diagnostic demandee\r\n");
        }
        return;
    }
    // Cette option reste accessible dans tous les modes : elle ne sauvegarde aucun réglage.
    if (!strncmp(line, "GPS_DEMO=", 9)) {
        // Les neuf caractères du préfixe sont suivis uniquement de 0 ou de 1.
        const char *value = line + 9;
        if (strcmp(value, "0") && strcmp(value, "1")) {
            output("ERREUR : GPS_DEMO=0 ou GPS_DEMO=1\r\n");
            return;
        }
        bool enabled = *value == '1';
        if (gps_demo != enabled) {
            gps_demo = enabled;
            // Repartir d'une acquisition complète avec le nouveau délai, sans publier le brouillon.
            cycle = false;
            force_measure = false;
            next_cycle = HAL_GetTick();
        }
        // Répéter la même commande confirme l'état sans recommencer l'attente des 15 s.
        output(gps_demo ? "GPS DEMONSTRATION active : position fictive 0,0 apres 15 s sans position\r\n"
                        : "GPS DEMONSTRATION desactive : fonctionnement normal\r\n");
        return;
    }
    if (mode != MODE_CONFIGURATION) {
        output("ERREUR : reglage reserve au mode CONFIGURATION\r\n");
        return;
    }
    if (!strcmp(line, "RESET")) {
        Config n;
        Config_Defaults(&n);
        if (Nv_Save(&n)) {
            cfg = n;
            output("OK RESET\r\n");
        } else
            output("ERREUR FLASH, configuration conservee\r\n");
        return;
    }
    char *eq = strchr(line, '=');
    if (!eq) {
        output("ERREUR syntaxe : HELP\r\n");
        return;
    }
    // *eq désigne le caractère trouvé ; *eq++ = 0 termine la clé puis avance le pointeur
    // vers la valeur. Les deux chaînes utilisent le même tampon command_line.
    *eq++ = 0;
    if (!strcmp(line, "CLOCK") || !strcmp(line, "DATE") || !strcmp(line, "DAY")) {
        output(rtc_command(line, eq) ? "OK RTC\r\n" : "ERREUR RTC ou format\r\n");
        return;
    }
    // Copie de travail : cfg n'est remplacée qu'après validation et sauvegarde réussies.
    Config n = cfg;
    if (!Config_Set(&n, line, eq)) {
        output("ERREUR parametre, valeur ou seuils\r\n");
        return;
    }
    if (!Nv_Save(&n)) {
        output("ERREUR FLASH : ancien reglage conserve\r\n");
        return;
    }
    cfg = n;
    output("OK parametre sauvegarde\r\n");
}
// Vide les octets disponibles sans attendre une nouvelle saisie ; now est en ms.
// CRLF ne doit exécuter qu'une commande. En cas de saturation, toute la ligne est refusée
// plutôt que d'exécuter une commande tronquée. Chaque octet reçu actualise l'inactivité.
static void console_process(uint32_t now) {
    int c;
    while ((c = Board_ConsoleGet()) >= 0) {
        last_activity = now;
        if (c == '\n' && last_was_cr) {
            last_was_cr = false;
            continue;
        }
        last_was_cr = c == '\r';
        if (c == '\r' || c == '\n') {
            if (command_overflow)
                output("ERREUR ligne trop longue\r\n");
            else {
                command_line[command_used] = 0;
                execute(command_line);
            }
            command_used = 0;
            command_overflow = false;
        } else if (c == 8 || c == 127) {
            if (command_used)
                command_used--;
        } else if (c >= 32 && c <= 126 && !command_overflow) {
            if (command_used < sizeof(command_line) - 1)
                command_line[command_used++] = c;
            else
                command_overflow = true;
        }
    }
}
/* Réinitialiser les statuts pour ne jamais republier une valeur ancienne comme mesure récente. */
// Prépare une nouvelle Measurement, mémorise son mode et programme la prochaine échéance.
// memset remet les valeurs à zéro, mais seuls les statuts diront si elles sont utilisables.
// DISABLED = désactivé par réglage ; TIMEOUT = encore attendu ; SKIPPED = omission volontaire.
static void begin_cycle(uint32_t now) {
    memset(&measurement, 0, sizeof(measurement));
    measurement.mode = mode;
    for (unsigned i = 0; i < SENSOR_COUNT; i++)
        measurement.status[i] = VALUE_DISABLED;
    if (cfg.lumin)
        measurement.status[SENSOR_LIGHT] = VALUE_TIMEOUT;
    if (cfg.temp)
        measurement.status[SENSOR_TEMP] = VALUE_TIMEOUT;
    if (cfg.hygr)
        measurement.status[SENSOR_HUM] = VALUE_TIMEOUT;
    if (cfg.pressure)
        measurement.status[SENSOR_PRESS] = VALUE_TIMEOUT;
    // Le premier cycle économique demande le GPS, puis un cycle sur deux.
    // Gps_Reset élimine la position mémorisée ; la réception UART reste active.
    request_gps = mode != MODE_ECONOMIC || economic_cycle % 2 == 0;
    economic_cycle++;
    measurement.status[SENSOR_GPS] = request_gps ? VALUE_TIMEOUT : VALUE_SKIPPED;
    cycle_start = now;
    light_due = env_due = now;
    env_pending = false;
    cycle = true;
    Gps_Reset();
    next_cycle = now + period();
}
/* Mémoriser les défauts par source, lire la RTC en fin de collecte et publier la mesure. */
// Termine la collecte dans la boucle principale ; now sert à reprogrammer la maintenance.
// Une valeur incohérente produit immédiatement son défaut ; un dépassement du délai d'attente doit se répéter.
// La date est lue en fin de collecte : elle ne date pas exactement chaque lecture de capteur.
static void finish_cycle(uint32_t now) {
    for (unsigned i = 0; i < SENSOR_COUNT; i++) {
        ValueStatus s = measurement.status[i];
        uint8_t bit = 1u << i;
        if (s == VALUE_DISABLED) {
            sensor_access &= ~bit;
            sensor_invalid &= ~bit;
            timeout_count[i] = 0;
            continue;
        }
        if (s == VALUE_SKIPPED)
            // Ne pas prendre une omission GPS volontaire pour un succès ou un nouvel échec.
            continue;
        if (s == VALUE_TIMEOUT) {
            if (timeout_count[i] < 2)
                timeout_count[i]++;
            if (timeout_count[i] >= 2)
                sensor_access |= bit;
            sensor_invalid &= ~bit;
        } else if (s == VALUE_INVALID) {
            sensor_invalid |= bit;
            sensor_access &= ~bit;
            timeout_count[i] = 0;
        } else {
            // OK et SIMULATED résolvent ce capteur seulement ; les autres gardent leurs défauts.
            sensor_access &= ~bit;
            sensor_invalid &= ~bit;
            timeout_count[i] = 0;
        }
    }
    measurement.date_valid = Time_Read(&measurement.date);
    errors = measurement.date_valid ? 0 : ERR_RTC;
    if (sensor_access & (1u << SENSOR_GPS))
        errors |= ERR_GPS;
    if (sensor_access & ~(1u << SENSOR_GPS))
        errors |= ERR_SENSOR_ACCESS;
    if (sensor_invalid)
        errors |= ERR_SENSOR_INVALID;
    if (mode == MODE_MAINTENANCE) {
        char line[256];
        /* Affichage lisible réservé à la console de maintenance. */
        if (Measurement_Format(&measurement, line, sizeof(line)) >= 0) {
            // fields contient des pointeurs dans line, pas huit copies de chaînes.
            // Les ';' deviennent des '\0' pour présenter les mêmes champs un par un.
            // Cette transformation locale ne change pas le format compact écrit sur la SD.
            char *fields[8];
            unsigned count = 1;
            fields[0] = line;
            line[strcspn(line, "\r\n")] = '\0';
            for (char *p = line; *p; p++) {
                if (*p == ';') {
                    *p = '\0';
                    if (count < 8)
                        fields[count++] = p + 1;
                }
            }
            if (count == 8) {
                printf("\r\n"
                       "Date / heure : %s\r\n"
                       "Mode         : %s\r\n"
                       "Temperature  : %s deg C\r\n"
                       "Humidite     : %s %%\r\n"
                       "Pression     : %s hPa\r\n"
                       "Luminosite   : %s / 1023\r\n"
                       "Latitude     : %s\r\n"
                       "Longitude    : %s\r\n\r\n",
                       fields[0], fields[1], fields[2], fields[3],
                       fields[4], fields[5], fields[6], fields[7]);
            }
        }
        // La maintenance relance une acquisition 1 s après sa fin, sans écrire de mesure sur SD.
        next_cycle = now + 1000;
    } else {
        // Storage_Write renvoie aussi ERR_RTC avant toute écriture si la date est invalide.
        // Le texte générique SD=ERREUR ne prouve donc pas une panne matérielle de la carte.
        // MEASURE en standard suit ce même chemin : il affiche le bilan, pas le bloc détaillé.
        sd_error = Storage_Write(&measurement, cfg.file_max);
        char s[96];
        snprintf(s, sizeof(s), "CYCLE %s SD=%s\r\n", mode_name(),
                 sd_error ? "ERREUR (STATUS)" : "OK");
        output(s);
    }
    errors |= sd_error;
    cycle = false;
}
/* Avancer les lectures sans attendre : lumière réessayée, conversion BME280 et GPS en continu. */
// Fait progresser les capteurs attendus ; now est en ms et aucune valeur n'est renvoyée.
// La collecte est découpée en étapes, même si chaque transaction HAL reste synchrone et bornée.
static void acquisition(uint32_t now) {
    // La soustraction non signée reste valide si le compteur milliseconde reboucle.
    uint32_t elapsed = now - cycle_start;
    uint32_t timeout_ms = (uint32_t)cfg.timeout_s * 1000u;
    // Prolonger l'attente GPS à 15 s ne doit pas prolonger celle des autres capteurs.
    bool sensors_active = !gps_demo || elapsed <= timeout_ms;
    if (sensors_active && cfg.lumin && measurement.status[SENSOR_LIGHT] == VALUE_TIMEOUT &&
        (int32_t)(now - light_due) >= 0) {
        uint16_t raw = Board_Light();
        light_due = now + 1000;
        if (raw != 0xffff) {
            // Ramène les 12 bits ADC (0..4095) vers 10 bits (0..1023), avec arrondi entier.
            // La conversion de type 32 bits permet de calculer le produit sans perdre la partie haute.
            measurement.light = ((uint32_t)raw * 1023 + 2047) / 4095;
            measurement.status[SENSOR_LIGHT] = VALUE_OK;
        }
    }
    bool env_needed = (cfg.temp && measurement.status[SENSOR_TEMP] == VALUE_TIMEOUT) ||
                      (cfg.hygr && measurement.status[SENSOR_HUM] == VALUE_TIMEOUT) ||
                      (cfg.pressure && measurement.status[SENSOR_PRESS] == VALUE_TIMEOUT);
    if (sensors_active && env_needed && (int32_t)(now - env_due) >= 0) {
        if (!env_ok)
            env_ok = Env_Init();
        if (!env_pending) {
            // Déclencher maintenant puis revenir après 100 ms laisse tourner boutons et GPS.
            // Les comparaisons signées d'échéance supposent un écart inférieur à 2^31 ms.
            env_pending = env_ok && Env_Begin();
            env_due = now + (env_pending ? 100 : 1000);
        } else {
            float t, h, p;
            // &t, &h et &p donnent au service trois emplacements de sortie : °C, % et hPa.
            bool ok = Env_Read(&t, &h, &p);
            env_pending = false;
            env_due = now + 1000;
            if (!ok)
                env_ok = false;
            else {
                measurement.temperature = t;
                measurement.humidity = h;
                measurement.pressure = p;
                bool valid_t = t >= cfg.temp_min && t <= cfg.temp_max;
                if (cfg.temp)
                    measurement.status[SENSOR_TEMP] = valid_t ? VALUE_OK : VALUE_INVALID;
                if (cfg.hygr)
                    measurement.status[SENSOR_HUM] =
                        valid_t && t >= cfg.hygr_mint && t <= cfg.hygr_maxt && h >= 0 && h <= 100
                            ? VALUE_OK
                            : VALUE_INVALID;
                if (cfg.pressure)
                    measurement.status[SENSOR_PRESS] =
                        p >= cfg.pressure_min && p <= cfg.pressure_max ? VALUE_OK : VALUE_INVALID;
            }
        }
    }
    // Une GGA valide doit être reçue depuis le début de ce cycle et rester récente.
    if (request_gps && measurement.status[SENSOR_GPS] == VALUE_TIMEOUT &&
        Gps_Take(cycle_start, &measurement.gps))
        measurement.status[SENSOR_GPS] = VALUE_OK;
    // Essayer la vraie position avant le remplacement, même à l'échéance des 15 secondes.
    // Le pilote GPS reste inchangé : aucune position fictive n'entre dans son cache.
    if (gps_demo && request_gps && measurement.status[SENSOR_GPS] == VALUE_TIMEOUT &&
        elapsed >= GPS_DEMO_TIMEOUT_MS) {
        // Remet aussi qualité et satellites à zéro : aucune réception valide n'est inventée.
        measurement.gps = (GpsFix){0};
        measurement.status[SENSOR_GPS] = VALUE_SIMULATED;
        // Effacer uniquement le défaut GPS et son historique d'échecs, même si un autre
        // capteur retarde encore la fin du cycle. La LED conserve les autres erreurs.
        uint8_t bit = 1u << SENSOR_GPS;
        sensor_access &= ~bit;
        sensor_invalid &= ~bit;
        timeout_count[SENSOR_GPS] = 0;
        errors &= ~ERR_GPS;
        output("GPS DEMONSTRATION : position fictive 0,0 (DEMO), aucune position satellite\r\n");
    }
    bool waiting = false;
    for (unsigned i = 0; i < SENSOR_COUNT; i++) {
        // Une attente expirée ne bloque plus la publication ; en démonstration seul
        // le GPS prend le délai fixe de 15 s. Un GPS ignoré en économie reste SKIPPED.
        uint32_t deadline = gps_demo && i == SENSOR_GPS ? GPS_DEMO_TIMEOUT_MS : timeout_ms;
        if (measurement.status[i] == VALUE_TIMEOUT && elapsed < deadline)
            waiting = true;
    }
    // Dès que chaque capteur a répondu ou épuisé son délai, publier une seule mesure.
    // Les capteurs encore en TIMEOUT seront rendus sous la forme NA par Measurement_Format.
    if (!waiting)
        finish_cycle(now);
}
// Appel unique après Global_Init : recharge les réglages, choisit le mode initial,
// initialise le BME280 et vérifie la RTC ; le bouton rouge au démarrage choisit configuration.
// Le montage SD est différé dans ce mode, mais l'initialisation BME280 a tout de même lieu.
void App_Init(void) {
    GroveRGB_Init();
    gps_demo = false;
    Config_Defaults(&cfg);
    bool stored = Nv_Load(&cfg);
    uint32_t now = HAL_GetTick();
    red = (Button){.raw = Board_Red(),
                   .stable = Board_Red(),
                   .sent = Board_Red(),
                   .changed = now,
                   .pressed = now};
    green = (Button){.raw = Board_Green(),
                     .stable = Board_Green(),
                     .sent = Board_Green(),
                     .changed = now,
                     .pressed = now};
    mode = red.stable ? MODE_CONFIGURATION : MODE_STANDARD;
    previous_mode = MODE_STANDARD;
    next_cycle = now;
    last_activity = now;
    output("\r\n" VERSION " LOT=" LOT "\r\n");
    output(stored ? "Configuration Flash chargee\r\n"
                  : "Defauts charges (aucune configuration Flash valide)\r\n");
    initialized = true;
    env_ok = Env_Init();
    DateTime d;
    if (!Time_Read(&d))
        errors |= ERR_RTC;
    if (mode != MODE_CONFIGURATION)
        sd_error = Storage_Mount() ? 0 : ERR_SD_IO;
    errors |= sd_error;
    char s[80];
    snprintf(s, sizeof(s), "MODE %s ; HELP pour les commandes\r\n", mode_name());
    output(s);
}
// Un passage de la boucle principale : services de fond, console, modes puis acquisition.
// now est une photographie du compteur ms ; la collecte relit ensuite HAL_GetTick.
void App_Process(uint32_t now) {
    App_Background();
    console_process(now);
    uint8_t event = events;
    events = 0;
    if (event & 1) {
        // Arbitrage réellement implémenté : rouge en économie revient au standard.
        // Un nouvel appui rouge depuis standard entre en maintenance ; le sujet attribue
        // aussi une entrée directe en maintenance depuis économie au même appui.
        if (mode == MODE_STANDARD || mode == MODE_ECONOMIC)
            set_mode(mode == MODE_STANDARD ? MODE_MAINTENANCE : MODE_STANDARD, now);
        else if (mode == MODE_MAINTENANCE)
            set_mode(previous_mode, now);
    } else if ((event & 2) && mode == MODE_STANDARD)
        set_mode(MODE_ECONOMIC, now);
    if (mode == MODE_CONFIGURATION) {
        // Le retour empêche begin_cycle/acquisition, mais App_Background et la console
        // ont déjà tourné : réception/décodage GPS, boutons et LED restent servis.
        // last_activity est actualisé par les caractères de console et les changements de mode.
        if (now - last_activity >= 1800000)
            set_mode(MODE_STANDARD, now);
        return;
    }
    if (!cycle && (force_measure || (int32_t)(now - next_cycle) >= 0)) {
        force_measure = false;
        begin_cycle(now);
    }
    if (cycle)
        acquisition(HAL_GetTick());
    led(HAL_GetTick());
}

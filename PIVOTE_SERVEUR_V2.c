/*
 * PIVOTE - SERVEUR / ADMINISTRATEUR - VERSION 2
 * Architecture client/serveur avec authentification synchronisee
 * Compile sous Windows avec Code::Blocks + MinGW (C99)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <locale.h>
#include "auth.h"

#pragma comment(lib, "ws2_32.lib")

#define MAX                100
#define PORT               8888
#define BUFFER             2048
#define FICHIER_SAUVEGARDE "vote_data.txt"
#define FICHIER_EXCEL      "resultats_vote.csv"
#define CSV_PATH           "users.csv"

/* =========================================================
 *  STRUCTURES
 * ========================================================= */
typedef struct {
    int  id;
    char nom[50];
    int  a_vote;
    int  vote_blanc;
    char username[AUTH_MAX_USERNAME + 1];
} Electeur;

typedef struct {
    int  id;
    char nom[50];
    int  voix;
} Candidat;

/* =========================================================
 *  VARIABLES GLOBALES
 * ========================================================= */
Electeur electeurs[MAX];
Candidat candidats[MAX];
int nbElecteurs        = 0;
int nbCandidats        = 0;
int voteOuvert         = 0;
int affichageAutoActif = 0;

static AuthUser adminConnecte;
static int      adminOK = 0;

/* =========================================================
 *  PROTOTYPES
 * ========================================================= */
void menuServeur(void);
void ajouterElecteur(void);
void afficherElecteurs(void);
void ajouterCandidat(void);
void afficherCandidats(void);
void ouvrirVote(void);
void fermerVote(void);
void afficherResultats(void);
void afficherStatistiques(void);
void lancerServeurReseau(void);
void sauvegarderDonnees(void);
void chargerDonnees(void);
void exporterVersExcel(void);

/* =========================================================
 *  HELPERS LECTURE
 * ========================================================= */
static void lire_ligne(const char *invite, char *buf, size_t taille)
{
    printf("%s", invite);
    if (fgets(buf, (int)taille, stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[len-1] = '\0';
    }
}

static void vider_buffer_stdin(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* =========================================================
 *  AFFICHAGE UTILISATEUR
 * ========================================================= */
static void afficher_utilisateur(const AuthUser *u)
{
    printf(" - %s (role=%s, actif=%s)\n",
           u->username, u->role, u->active ? "oui" : "non");
}

/* =========================================================
 *  MODULE AUTH - fonctions admin
 * ========================================================= */

/*
 * Quand l'admin ajoute un electeur, il cree en meme temps
 * son compte de connexion dans users.csv (role = "votant").
 */
void ajouterElecteur(void)
{
    if (nbElecteurs >= MAX) {
        printf("Nombre maximum d'electeurs atteint.\n");
        return;
    }

    Electeur e;
    char username[AUTH_MAX_USERNAME + 1];
    char password[AUTH_MAX_PASSWORD + 1];

    printf("ID numerique de l'electeur : ");
    scanf("%d", &e.id);
    vider_buffer_stdin();

    printf("Nom de l'electeur : ");
    fgets(e.nom, sizeof(e.nom), stdin);
    {
        size_t l = strlen(e.nom);
        if (l > 0 && e.nom[l-1] == '\n') e.nom[l-1] = '\0';
    }

    lire_ligne("Identifiant de connexion (login) : ", username, sizeof(username));
    lire_ligne("Mot de passe initial             : ", password, sizeof(password));

    /* Verification unicite ID numerique */
    for (int i = 0; i < nbElecteurs; i++) {
        if (electeurs[i].id == e.id) {
            printf("Erreur : un electeur avec l'ID %d existe deja.\n", e.id);
            return;
        }
    }

    /* Creation du compte dans users.csv */
    AuthStatus st = auth_register_user(CSV_PATH, username, password, "votant");
    if (st == AUTH_ERR_EXISTS) {
        printf("Erreur : un compte avec l'identifiant '%s' existe deja.\n", username);
        return;
    } else if (st != AUTH_OK) {
        printf("Erreur lors de la creation du compte (code=%d).\n", st);
        return;
    }

    e.a_vote     = 0;
    e.vote_blanc = 0;
    strncpy(e.username, username, AUTH_MAX_USERNAME);
    e.username[AUTH_MAX_USERNAME] = '\0';

    electeurs[nbElecteurs++] = e;
    printf("Electeur '%s' (login: %s) enregistre avec succes.\n", e.nom, username);
}

/* Creer un compte supplementaire (autre admin, etc.) */
static void menu_inscription_admin(void)
{
    char username[AUTH_MAX_USERNAME + 1];
    char password[AUTH_MAX_PASSWORD + 1];
    char role[AUTH_MAX_ROLE + 1];

    lire_ligne("Identifiant : ", username, sizeof(username));
    lire_ligne("Mot de passe : ", password, sizeof(password));
    lire_ligne("Role (votant/admin) : ", role, sizeof(role));

    AuthStatus st = auth_register_user(CSV_PATH, username, password, role);
    if (st == AUTH_OK)
        printf("Utilisateur cree avec succes.\n");
    else if (st == AUTH_ERR_EXISTS)
        printf("Erreur : un utilisateur avec cet identifiant existe deja.\n");
    else
        printf("Erreur lors de la creation de l'utilisateur (code=%d).\n", st);
}

/* Changer son propre mot de passe (avec verification de l'ancien) */
static void menu_changer_mdp(void)
{
    char username[AUTH_MAX_USERNAME + 1];
    char old_password[AUTH_MAX_PASSWORD + 1];
    char new_password[AUTH_MAX_PASSWORD + 1];

    lire_ligne("Identifiant : ", username, sizeof(username));
    lire_ligne("Ancien mot de passe : ", old_password, sizeof(old_password));
    lire_ligne("Nouveau mot de passe : ", new_password, sizeof(new_password));

    AuthStatus st = auth_change_password(CSV_PATH, username, old_password, new_password);
    if (st == AUTH_OK)
        printf("Mot de passe mis a jour.\n");
    else if (st == AUTH_ERR_NOTFOUND)
        printf("Utilisateur inconnu.\n");
    else if (st == AUTH_ERR_INVALID)
        printf("Ancien mot de passe incorrect.\n");
    else
        printf("Erreur lors du changement de mot de passe (code=%d).\n", st);
}

/*
 * Reinitialisation FORCEE du mot de passe par l'admin.
 * Ne demande PAS l'ancien mot de passe.
 * A utiliser quand un electeur a oublie son mot de passe.
 * NULL passe a auth_change_password = reinitialisation sans controle.
 */
static void menu_reinitialiser_mdp(void)
{
    char username[AUTH_MAX_USERNAME + 1];
    char new_password[AUTH_MAX_PASSWORD + 1];

    printf("\n[REINITIALISATION MOT DE PASSE]\n");
    printf("Cette action ne necessite pas l'ancien mot de passe.\n");
    printf("Reservee a l'administrateur pour aider un electeur bloque.\n\n");

    lire_ligne("Identifiant de l'electeur : ", username, sizeof(username));
    lire_ligne("Nouveau mot de passe       : ", new_password, sizeof(new_password));

    AuthStatus st = auth_change_password(CSV_PATH, username, NULL, new_password);
    if (st == AUTH_OK)
        printf("Mot de passe de '%s' reinitialise avec succes.\n", username);
    else if (st == AUTH_ERR_NOTFOUND)
        printf("Utilisateur inconnu.\n");
    else
        printf("Erreur lors de la reinitialisation (code=%d).\n", st);
}

static void menu_activation(int activer)
{
    char username[AUTH_MAX_USERNAME + 1];
    lire_ligne("Identifiant : ", username, sizeof(username));

    AuthStatus st = auth_set_active(CSV_PATH, username, activer);
    if (st == AUTH_OK)
        printf("Compte %s %s.\n", username, activer ? "active" : "desactive");
    else if (st == AUTH_ERR_NOTFOUND)
        printf("Utilisateur inconnu.\n");
    else
        printf("Erreur lors de la mise a jour du compte (code=%d).\n", st);
}

static void menu_lister(void)
{
    AuthUser *users = NULL;
    size_t    count = 0;

    AuthStatus st = auth_list_users(CSV_PATH, &users, &count);
    if (st != AUTH_OK) {
        printf("Impossible de lire la liste des utilisateurs (code=%d).\n", st);
        return;
    }
    printf("Utilisateurs (%zu) :\n", count);
    for (size_t i = 0; i < count; ++i)
        afficher_utilisateur(&users[i]);
    auth_free_user_list(users);
}

/* =========================================================
 *  THREAD SERVEUR RESEAU
 *
 *  Protocole :
 *    Client -> Serveur : "LOGIN <username> <password> <idElecteur> <idCandidat>"
 *    Serveur -> Client : "OK" ou "ERREUR"
 *
 *  Verifications dans l'ordre :
 *    1. login + mot de passe corrects (auth_authenticate)
 *    2. role = "votant"
 *    3. ID electeur correspond au bon login
 *    4. electeur n'a pas encore vote
 *    5. vote est ouvert
 * ========================================================= */
DWORD WINAPI threadServeurReseau(LPVOID arg)
{
    WSADATA wsa;
    SOCKET  serveur, client;
    struct sockaddr_in addr;
    char   buffer[BUFFER];
    char   listeCandidatsStr[BUFFER];
    int    addrlen = sizeof(addr);

    WSAStartup(MAKEWORD(2,2), &wsa);
    serveur = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(serveur, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[ERREUR] Impossible de lier le port %d.\n", PORT);
        return 1;
    }
    listen(serveur, 5);
    printf(">> Serveur reseau ACTIF sur le port %d.\n", PORT);

    while (1) {
        client = accept(serveur, (struct sockaddr*)&addr, &addrlen);
        if (client == INVALID_SOCKET) continue;

        /* -----------------------------------------------
         * ETAPE 1 : Authentification
         * Format recu : "AUTH <username> <password>"
         * ----------------------------------------------- */
        int recv_size = recv(client, buffer, BUFFER - 1, 0);
        if (recv_size <= 0) { closesocket(client); continue; }
        buffer[recv_size] = '\0';

        char cmd[16];
        char username[AUTH_MAX_USERNAME + 1];
        char password[AUTH_MAX_PASSWORD + 1];

        int parsed = sscanf(buffer, "%15s %64s %64s", cmd, username, password);

        if (parsed != 3 || strcmp(cmd, "AUTH") != 0) {
            send(client, "AUTH_FAIL", 9, 0);
            closesocket(client);
            continue;
        }

        AuthUser uAuth;
        AuthStatus stAuth = auth_authenticate(CSV_PATH, username, password, &uAuth);

        if (stAuth != AUTH_OK || strcmp(uAuth.role, "votant") != 0) {
            send(client, "AUTH_FAIL", 9, 0);
            closesocket(client);
            continue;
        }

        /* Authentification OK */
        send(client, "AUTH_OK", 7, 0);

        /* -----------------------------------------------
         * ETAPE 2 : Envoi liste candidats
         * ----------------------------------------------- */
        strcpy(listeCandidatsStr, "\n--- LISTE DES CANDIDATS ---\n");
        char ligne[100];
        for (int k = 0; k < nbCandidats; k++) {
            sprintf(ligne, "[%d] %s\n", candidats[k].id, candidats[k].nom);
            strcat(listeCandidatsStr, ligne);
        }
        strcat(listeCandidatsStr, "[0] VOTE BLANC\n---------------------------\n");
        send(client, listeCandidatsStr, strlen(listeCandidatsStr), 0);

        /* -----------------------------------------------
         * ETAPE 3 : Reception du vote
         * Format recu : "VOTE <idElecteur> <idCandidat>"
         * ----------------------------------------------- */
        recv_size = recv(client, buffer, BUFFER - 1, 0);
        if (recv_size <= 0) { closesocket(client); continue; }
        buffer[recv_size] = '\0';

        char cmd2[16];
        int  idE = -1, idC = -1;
        sscanf(buffer, "%15s %d %d", cmd2, &idE, &idC);

        int ok = 0;

        if (strcmp(cmd2, "VOTE") == 0 && voteOuvert) {
            for (int i = 0; i < nbElecteurs; i++) {
                if (electeurs[i].id == idE
                    && strcmp(electeurs[i].username, username) == 0
                    && electeurs[i].a_vote == 0)
                {
                    int candidatTrouve = 0;
                    for (int j = 0; j < nbCandidats; j++) {
                        if (candidats[j].id == idC) {
                            candidats[j].voix++;
                            candidatTrouve = 1;
                            break;
                        }
                    }
                    electeurs[i].vote_blanc = candidatTrouve ? 0 : 1;
                    electeurs[i].a_vote = 1;
                    ok = 1;
                    break;
                }
            }
        }

        if (ok) {
            send(client, "OK", 2, 0);
            sauvegarderDonnees();
            exporterVersExcel();
        } else {
            send(client, "ERREUR", 6, 0);
        }

        closesocket(client);
    }
    return 0;
}
/* =========================================================
 *  THREAD AFFICHAGE TEMPS REEL
 * ========================================================= */
DWORD WINAPI threadAffichageTempsReel(LPVOID arg)
{
    while (affichageAutoActif) {
        system("cls");
        printf("===== CONTROLE EN TEMPS REEL  =====\n");
        afficherResultats();
        printf("\n");
        afficherStatistiques();
        printf("\n[INFO] Fichier Excel mis a jour automatiquement.\n");
        printf("Appuie sur une touche du menu pour quitter...\n");
        Sleep(3000);
    }
    return 0;
}

/* =========================================================
 *  GESTION FICHIERS
 * ========================================================= */
void sauvegarderDonnees(void)
{
    FILE *f = fopen(FICHIER_SAUVEGARDE, "w");
    if (!f) return;
    fprintf(f, "%d\n%d\n", voteOuvert, nbElecteurs);
    for (int i = 0; i < nbElecteurs; i++)
        fprintf(f, "%d %s %d %d %s\n",
                electeurs[i].id, electeurs[i].nom,
                electeurs[i].a_vote, electeurs[i].vote_blanc,
                electeurs[i].username);
    fprintf(f, "%d\n", nbCandidats);
    for (int i = 0; i < nbCandidats; i++)
        fprintf(f, "%d %s %d\n", candidats[i].id, candidats[i].nom, candidats[i].voix);
    fclose(f);
}

void exporterVersExcel(void)
{
    FILE *f = fopen(FICHIER_EXCEL, "w");
    if (!f) return;
    fprintf(f, "ID Candidat;Nom Candidat;Nombre de Voix\n");
    for (int i = 0; i < nbCandidats; i++)
        fprintf(f, "%d;%s;%d\n", candidats[i].id, candidats[i].nom, candidats[i].voix);
    int blancs = 0;
    for (int i = 0; i < nbElecteurs; i++) if (electeurs[i].vote_blanc) blancs++;
    fprintf(f, "0;VOTE BLANC;%d\n", blancs);
    fclose(f);
}

void chargerDonnees(void)
{
    FILE *f = fopen(FICHIER_SAUVEGARDE, "r");
    if (!f) return;
    fscanf(f, "%d", &voteOuvert);
    fscanf(f, "%d", &nbElecteurs);
    for (int i = 0; i < nbElecteurs; i++) {
        fscanf(f, "%d %s %d %d %s",
               &electeurs[i].id, electeurs[i].nom,
               &electeurs[i].a_vote, &electeurs[i].vote_blanc,
               electeurs[i].username);
    }
    fscanf(f, "%d", &nbCandidats);
    for (int i = 0; i < nbCandidats; i++)
        fscanf(f, "%d %s %d", &candidats[i].id, candidats[i].nom, &candidats[i].voix);
    fclose(f);
    printf(">> Donnees chargees.\n");
}

/* =========================================================
 *  FONCTIONS LOGIQUES (printfs originaux conserves)
 * ========================================================= */
void afficherElecteurs(void)
{
    for (int i = 0; i < nbElecteurs; i++)
        printf("ID:%d | %s (login:%s) | A vote: %s\n",
               electeurs[i].id, electeurs[i].nom,
               electeurs[i].username,
               electeurs[i].a_vote ? "OUI" : "NON");
}

void ajouterCandidat(void)
{
    if (nbCandidats >= MAX) return;
    Candidat c;
    printf("ID : ");
    scanf("%d", &c.id);
    vider_buffer_stdin();
    printf("Nom : ");
    fgets(c.nom, sizeof(c.nom), stdin);
    {
        size_t l = strlen(c.nom);
        if (l > 0 && c.nom[l-1] == '\n') c.nom[l-1] = '\0';
    }
    c.voix = 0;
    candidats[nbCandidats++] = c;
    printf("Candidat ajoute.\n");
}

void afficherCandidats(void)
{
    for (int i = 0; i < nbCandidats; i++)
        printf("ID:%d | %s | Voix: %d\n", candidats[i].id, candidats[i].nom, candidats[i].voix);
}

void ouvrirVote(void)
{
    voteOuvert = 1;
    printf("Vote OUVERT.\n");
}

void fermerVote(void)
{
    voteOuvert = 0;
    printf("Vote FERME.\n");
}

void afficherResultats(void)
{
    for (int i = 0; i < nbCandidats; i++)
        printf("%s : %d voix\n", candidats[i].nom, candidats[i].voix);
}

void afficherStatistiques(void)
{
    int v = 0, b = 0;
    for (int i = 0; i < nbElecteurs; i++) {
        if (electeurs[i].a_vote) {
            v++;
            if (electeurs[i].vote_blanc) b++;
        }
    }
    printf("Votants: %d / %d | Votes blancs: %d\n", v, nbElecteurs, b);
}

void lancerServeurReseau(void)
{
    HANDLE thread = CreateThread(NULL, 0, threadServeurReseau, NULL, 0, NULL);
    if (!thread) {
        printf("Erreur thread reseau.\n");
        return;
    }
    affichageAutoActif = 1;
    CreateThread(NULL, 0, threadAffichageTempsReel, NULL, 0, NULL);
    printf("Mode reseau actif. Appuyez sur 0 pour quitter proprement.\n");
}

/* =========================================================
 *  MENU GESTION DES COMPTES (sous-menu admin)
 * ========================================================= */
static void menuGestionComptes(void)
{
    int choix;
    char buf[32];
    do {
        printf("\n=== Gestion des comptes ===\n");
        printf("1. Creer un compte (admin/autre)\n");
        printf("2. Changer un mot de passe\n");
        printf("3. Activer un compte\n");
        printf("4. Desactiver un compte\n");
        printf("5. Lister les utilisateurs\n");
        printf("6. Reinitialiser le mot de passe d'un electeur\n");
        printf("0. Retour\n");
        lire_ligne("Choix : ", buf, sizeof(buf));
        if (sscanf(buf, "%d", &choix) != 1) choix = -1;
        switch (choix) {
            case 1: menu_inscription_admin();  break;
            case 2: menu_changer_mdp();        break;
            case 3: menu_activation(1);        break;
            case 4: menu_activation(0);        break;
            case 5: menu_lister();             break;
            case 6: menu_reinitialiser_mdp();  break;
            case 0: break;
            default: printf("Choix invalide.\n"); break;
        }
    } while (choix != 0);
}

/* =========================================================
 *  MENU SERVEUR PRINCIPAL
 * ========================================================= */
void menuServeur(void)
{
    int choix;
    do {
        printf("\n===== MENU PIVOTE ADMINISTRATEUR =====\n");
        printf("1. Ajouter un electeur\n");
        printf("2. Afficher les electeurs\n");
        printf("3. Ajouter un candidat\n");
        printf("4. Afficher les candidats\n");
        printf("5. Ouvrir le vote\n");
        printf("6. Fermer le vote\n");
        printf("7. Les resultats\n");
        printf("8. Les Statistiques\n");
        printf("9. Lancer le mode RESEAU\n");
        printf("10. Exporter vers Excel \n");
        printf("11. Gestion des comptes\n");
        printf("0. Quitter ET REINITIALISER\n");
        printf("Choix : ");
        scanf("%d", &choix);
        vider_buffer_stdin();

        switch (choix) {
        case 1:  ajouterElecteur();      sauvegarderDonnees(); break;
        case 2:  afficherElecteurs();    break;
        case 3:  ajouterCandidat();      sauvegarderDonnees(); break;
        case 4:  afficherCandidats();    break;
        case 5:  ouvrirVote();           sauvegarderDonnees(); break;
        case 6:  fermerVote();           sauvegarderDonnees(); break;
        case 7:  afficherResultats();    break;
        case 8:  afficherStatistiques(); break;
        case 9:  lancerServeurReseau();  break;
        case 10:
            exporterVersExcel();
            printf("Fichier Excel genere !\n");
            break;
        case 11:
            menuGestionComptes();
            break;
        case 0:
            affichageAutoActif = 0;
            remove(FICHIER_SAUVEGARDE);
            printf(">> Session terminee. Fichiers de sauvegarde supprimes.\n");
            break;
        default:
            printf("Choix invalide.\n");
            break;
        }
    } while (choix != 0);
}

/* =========================================================
 *  ECRAN DE CONNEXION ADMINISTRATEUR
 *  - Premiere utilisation : creation du compte admin principal
 *  - Ensuite : login obligatoire avec role "admin"
 *  - 3 tentatives maximum
 * ========================================================= */
static int ecranConnexionAdmin(void)
{
    char username[AUTH_MAX_USERNAME + 1];
    char password[AUTH_MAX_PASSWORD + 1];

    printf("\n===================================================\n");
    printf("          PIVOTE - ESPACE ADMINISTRATEUR\n");
    printf("===================================================\n");

    /* Verifier si un admin existe deja */
    AuthUser *users = NULL;
    size_t    count = 0;
    int adminExiste = 0;
    if (auth_list_users(CSV_PATH, &users, &count) == AUTH_OK) {
        for (size_t i = 0; i < count; i++) {
            if (strcmp(users[i].role, "admin") == 0) {
                adminExiste = 1;
                break;
            }
        }
        auth_free_user_list(users);
    }

    if (!adminExiste) {
        printf("\n[PREMIERE UTILISATION] Aucun administrateur trouve.\n");
        printf("Veuillez creer le compte administrateur principal :\n");
        lire_ligne("Identifiant admin  : ", username, sizeof(username));
        lire_ligne("Mot de passe admin : ", password, sizeof(password));
        AuthStatus st = auth_register_user(CSV_PATH, username, password, "admin");
        if (st != AUTH_OK) {
            printf("Erreur creation admin (code=%d). Arret.\n", st);
            return 0;
        }
        printf("Compte admin cree. Veuillez vous connecter.\n\n");
    }

    int tentatives = 3;
    while (tentatives > 0) {
        lire_ligne("Identifiant : ", username, sizeof(username));
        lire_ligne("Mot de passe : ", password, sizeof(password));

        AuthStatus st = auth_authenticate(CSV_PATH, username, password, &adminConnecte);
        if (st == AUTH_OK && strcmp(adminConnecte.role, "admin") == 0) {
            printf("\nAuthentification reussie. Bonjour %s !\n", adminConnecte.username);
            return 1;
        } else {
            tentatives--;
            if (tentatives > 0)
                printf("Identifiants incorrects ou compte non-admin. %d tentative(s) restante(s).\n", tentatives);
            else
                printf("Trop de tentatives. Acces refuse.\n");
        }
    }
    return 0;
}

/* =========================================================
 *  MAIN
 * ========================================================= */
int main(void)
{
    /* Support des accents sous Windows */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    setlocale(LC_ALL, "");

    /* Initialisation du fichier utilisateurs */
    AuthStatus st = auth_init(CSV_PATH);
    if (st != AUTH_OK) {
        printf("Erreur d'initialisation du fichier utilisateurs (code=%d).\n", st);
        return 1;
    }

    /* Connexion administrateur obligatoire */
    adminOK = ecranConnexionAdmin();
    if (!adminOK) {
        printf("Impossible de se connecter. Fermeture.\n");
        system("pause");
        return 1;
    }

    /* Chargement des donnees de vote existantes */
    chargerDonnees();

    /* Menu principal administrateur */
    menuServeur();

    return 0;
}

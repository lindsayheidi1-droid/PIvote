/**
 * @file serveur.h
 * @brief Signatures des fonctions du SERVEUR PIVOTE V2 (administrateur).
 *
 * Compilation (MinGW / Code::Blocks, C99) :
 * gcc -std=c99 -Wall serveur_impl.c serveur_main.c auth.c -o serveur.exe -lws2_32
 */

#ifndef SERVEUR_H
#define SERVEUR_H

#include "auth.h"
#include <winsock2.h>
#include <windows.h>
#include <stddef.h>

/* =========================================================
 * CONSTANTES
 * ========================================================= */
#define MAX                100
#define PORT               8888
#define BUFFER             2048
#define FICHIER_SAUVEGARDE "vote_data.txt"
#define FICHIER_EXCEL      "resultats_vote.csv"
#define FICHIER_RAPPORT    "rapport_final.txt"
#define CSV_PATH           "users.csv"

/* =========================================================
 * STRUCTURES
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
 * VARIABLES GLOBALES (extern)
 * ========================================================= */
extern Electeur electeurs[MAX];
extern Candidat candidats[MAX];
extern int nbElecteurs;
extern int nbCandidats;
extern int voteOuvert;
extern int affichageAutoActif;

/* =========================================================
 * 1. HELPERS CONSOLE
 * ========================================================= */
void lire_ligne_srv(const char *invite, char *buf, size_t taille);
void vider_buffer_stdin(void);

/* =========================================================
 * 2. GESTION DES ELECTEURS ET CANDIDATS
 * ========================================================= */
void ajouterElecteur(void);
void afficherElecteurs(void);
void ajouterCandidat(void);
void afficherCandidats(void);

/* =========================================================
 * 3. LOGIQUE DE VOTE
 * ========================================================= */
void ouvrirVote(void);
void fermerVote(void);
void afficherResultats(void);
void afficherStatistiques(void);

/* =========================================================
 * 4. NOUVELLES FONCTIONNALITES
 * ========================================================= */
/**
 * @brief Affiche les resultats en barres ASCII avec pourcentages.
 *   Candidat A [################  ] 16 voix (80%)
 *   Candidat B [####              ]  4 voix (20%)
 */
void afficherBarresASCII(void);

/**
 * @brief Trouve et affiche le(s) gagnant(s) du scrutin.
 *        Gere les cas d'egalite. Appele auto a la fermeture.
 */
void afficherGagnant(void);

/**
 * @brief Genere rapport_final.txt : date/heure, resultats,
 *        gagnant, taux de participation, votes blancs.
 *        Appele automatiquement a la fermeture du vote.
 */
void genererRapportFinal(void);

/* =========================================================
 * 5. PERSISTANCE DES DONNEES
 * ========================================================= */
void sauvegarderDonnees(void);
void chargerDonnees(void);
void exporterVersExcel(void);

/* =========================================================
 * 6. SERVEUR RESEAU (threads Windows)
 * Protocole :
 *   Client -> "AUTH <username> <password>"
 *   Serveur -> "AUTH_OK" ou "AUTH_FAIL"
 *   Serveur -> liste des candidats
 *   Client -> "VOTE <idElecteur> <idCandidat>"
 *   Serveur -> "OK" ou "ERREUR"
 * ========================================================= */
DWORD WINAPI threadServeurReseau(LPVOID arg);
DWORD WINAPI threadAffichageTempsReel(LPVOID arg);
void lancerServeurReseau(void);

/* =========================================================
 * 7. GESTION DES COMPTES UTILISATEURS
 * ========================================================= */
void menu_inscription_admin(void);
void menu_changer_mdp(void);
void menu_reinitialiser_mdp(void);
void menu_activation(int activer);
void menu_lister(void);
void menuGestionComptes(void);

/* =========================================================
 * 8. MENU PRINCIPAL ADMINISTRATEUR
 * ========================================================= */
void menuServeur(void);

/* =========================================================
 * 9. CONNEXION ADMINISTRATEUR
 * ========================================================= */
int ecranConnexionAdmin(void);

#endif /* SERVEUR_H */

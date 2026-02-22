/**
 * @file auth.h
 * @brief Bibliothèque d'authentification simple basée sur un fichier CSV.
 *
 * Le fichier CSV contient une ligne par utilisateur au format :
 *   identifiant;mot_de_passe;role;actif
 * où :
 *   - identifiant : nom d'utilisateur (unique)
 *   - mot_de_passe : mot de passe en clair (démonstration, NON sécurisé)
 *   - role : chaîne libre (ex: "admin", "user")
 *   - actif : 1 si le compte est actif, 0 sinon
 *
 * Cette API est utilisable aussi bien en mode console que depuis une
 * interface graphique en C (GTK, Win32, etc.) car elle ne dépend pas de l'IHM.
 */

#ifndef AUTH_H
#define AUTH_H

#include <stddef.h>

/** Taille maximale d'un identifiant utilisateur (sans le '\0'). */
#define AUTH_MAX_USERNAME 64

/** Taille maximale d'un mot de passe (sans le '\0'). */
#define AUTH_MAX_PASSWORD 64

/** Taille maximale d'un rôle (sans le '\0'). */
#define AUTH_MAX_ROLE 32

/**
 * @brief Structure représentant un utilisateur.
 */
typedef struct
{
    char username[AUTH_MAX_USERNAME + 1];  /**< Identifiant (login). */
    char password[AUTH_MAX_PASSWORD + 1];  /**< Mot de passe en clair (démonstration). */
    char role[AUTH_MAX_ROLE + 1];          /**< Rôle de l'utilisateur. */
    int  active;                           /**< 1 si actif, 0 si désactivé. */
} AuthUser;

/**
 * @brief Codes de retour génériques de la bibliothèque.
 */
typedef enum
{
    AUTH_OK = 0,          /**< Opération réussie. */
    AUTH_ERR_IO = -1,     /**< Erreur d'entrée/sortie (fichier). */
    AUTH_ERR_FORMAT = -2, /**< Format CSV invalide. */
    AUTH_ERR_NOTFOUND = -3, /**< Utilisateur non trouvé. */
    AUTH_ERR_EXISTS = -4,   /**< Utilisateur déjà existant. */
    AUTH_ERR_INVALID = -5   /**< Paramètres invalides ou mot de passe incorrect. */
} AuthStatus;

/**
 * @brief Initialise le fichier d'utilisateurs si nécessaire.
 *
 * Si le fichier n'existe pas, il est créé vide (sans en-tête).
 *
 * @param csv_path Chemin du fichier CSV des utilisateurs.
 * @return AUTH_OK en cas de succès, AUTH_ERR_IO sinon.
 */
AuthStatus auth_init(const char *csv_path);

/**
 * @brief Inscrit (enregistre) un nouvel utilisateur.
 *
 * L'identifiant doit être unique. Le mot de passe est stocké tel quel
 * (pour un vrai projet, utiliser un hachage sécurisé).
 *
 * @param csv_path Chemin du fichier CSV des utilisateurs.
 * @param username Identifiant du nouvel utilisateur.
 * @param password Mot de passe du nouvel utilisateur.
 * @param role     Rôle (ex: "user", "admin").
 * @return AUTH_OK si l'utilisateur a été ajouté,
 *         AUTH_ERR_EXISTS si l'identifiant existe déjà,
 *         AUTH_ERR_IO ou AUTH_ERR_FORMAT en cas de problème de fichier.
 */
AuthStatus auth_register_user(const char *csv_path,
                              const char *username,
                              const char *password,
                              const char *role);

/**
 * @brief Authentifie un utilisateur à partir de l'identifiant et du mot de passe.
 *
 * @param csv_path Chemin du fichier CSV des utilisateurs.
 * @param username Identifiant de l'utilisateur.
 * @param password Mot de passe fourni.
 * @param out_user Si non NULL et si l'authentification réussit, les
 *                 informations de l'utilisateur sont copiées dans cette structure.
 * @return AUTH_OK si authentifié avec succès et compte actif,
 *         AUTH_ERR_NOTFOUND si l'utilisateur n'existe pas,
 *         AUTH_ERR_INVALID si le mot de passe est incorrect ou le compte inactif,
 *         AUTH_ERR_IO ou AUTH_ERR_FORMAT en cas d'erreur sur le fichier.
 */
AuthStatus auth_authenticate(const char *csv_path,
                             const char *username,
                             const char *password,
                             AuthUser *out_user);

/**
 * @brief Change le mot de passe d'un utilisateur.
 *
 * @param csv_path Chemin du fichier CSV des utilisateurs.
 * @param username Identifiant de l'utilisateur.
 * @param old_password Ancien mot de passe (peut être NULL pour forcer sans contrôle).
 * @param new_password Nouveau mot de passe.
 * @return AUTH_OK si le mot de passe a été changé,
 *         AUTH_ERR_NOTFOUND si l'utilisateur n'existe pas,
 *         AUTH_ERR_INVALID si l'ancien mot de passe est incorrect,
 *         AUTH_ERR_IO ou AUTH_ERR_FORMAT en cas d'erreur de fichier.
 */
AuthStatus auth_change_password(const char *csv_path,
                                const char *username,
                                const char *old_password,
                                const char *new_password);

/**
 * @brief Active ou désactive un compte utilisateur.
 *
 * @param csv_path Chemin du fichier CSV des utilisateurs.
 * @param username Identifiant de l'utilisateur.
 * @param active   1 pour activer, 0 pour désactiver.
 * @return AUTH_OK si succès,
 *         AUTH_ERR_NOTFOUND si l'utilisateur n'existe pas,
 *         AUTH_ERR_IO ou AUTH_ERR_FORMAT sinon.
 */
AuthStatus auth_set_active(const char *csv_path,
                           const char *username,
                           int active);

/**
 * @brief Récupère la liste complète des utilisateurs.
 *
 * Cette fonction alloue dynamiquement un tableau d'utilisateurs qui devra
 * être libéré par l'appelant via `auth_free_user_list()`.
 *
 * @param csv_path Chemin du fichier CSV des utilisateurs.
 * @param out_users Pointeur vers un tableau alloué d'utilisateurs (en sortie).
 * @param out_count Nombre d'utilisateurs dans le tableau (en sortie).
 * @return AUTH_OK si succès,
 *         AUTH_ERR_IO ou AUTH_ERR_FORMAT en cas de problème.
 */
AuthStatus auth_list_users(const char *csv_path,
                           AuthUser **out_users,
                           size_t *out_count);

/**
 * @brief Libère un tableau d'utilisateurs alloué par `auth_list_users()`.
 *
 * @param users Tableau à libérer (peut être NULL).
 */
void auth_free_user_list(AuthUser *users);

#endif /* AUTH_H */


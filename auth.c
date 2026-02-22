/**
 * @file auth.c
 * @brief Implémentation de la bibliothèque d'authentification simple basée sur un CSV.
 */

#include "auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Taille maximale d'une ligne lue dans le CSV. */
#define AUTH_MAX_LINE 512

/**
 * @brief Supprime le '\n' final éventuel d'une chaîne.
 */
static void auth_chomp(char *s)
{
    if (!s) return;
    size_t len = strlen(s);
    if (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
    {
        s[len - 1] = '\0';
        /* Gestion CRLF: si encore un '\r' */
        len = strlen(s);
        if (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        {
            s[len - 1] = '\0';
        }
    }
}

/**
 * @brief Parse une ligne CSV au format "username;password;role;active".
 *
 * @param line Ligne modifiable (sera modifiée par strtok).
 * @param out_user Utilisateur rempli en sortie.
 * @return AUTH_OK si succès, AUTH_ERR_FORMAT sinon.
 */
static AuthStatus auth_parse_line(char *line, AuthUser *out_user)
{
    if (!line || !out_user)
        return AUTH_ERR_INVALID;

    auth_chomp(line);

    char *username = strtok(line, ";");
    char *password = strtok(NULL, ";");
    char *role     = strtok(NULL, ";");
    char *active_s = strtok(NULL, ";");

    if (!username || !password || !role || !active_s)
        return AUTH_ERR_FORMAT;

    strncpy(out_user->username, username, AUTH_MAX_USERNAME);
    out_user->username[AUTH_MAX_USERNAME] = '\0';

    strncpy(out_user->password, password, AUTH_MAX_PASSWORD);
    out_user->password[AUTH_MAX_PASSWORD] = '\0';

    strncpy(out_user->role, role, AUTH_MAX_ROLE);
    out_user->role[AUTH_MAX_ROLE] = '\0';

    out_user->active = atoi(active_s) ? 1 : 0;

    return AUTH_OK;
}

/**
 * @brief Écrit un utilisateur au format CSV sur un flux ouvert.
 *
 * @param f Flux ouvert en écriture.
 * @param u Utilisateur à écrire.
 * @return AUTH_OK si succès, AUTH_ERR_IO sinon.
 */
static AuthStatus auth_fprint_user(FILE *f, const AuthUser *u)
{
    if (fprintf(f, "%s;%s;%s;%d\n",
                u->username,
                u->password,
                u->role,
                u->active ? 1 : 0) < 0)
    {
        return AUTH_ERR_IO;
    }
    return AUTH_OK;
}

AuthStatus auth_init(const char *csv_path)
{
    if (!csv_path)
        return AUTH_ERR_INVALID;

    FILE *f = fopen(csv_path, "r");
    if (f)
    {
        /* Le fichier existe déjà. */
        fclose(f);
        return AUTH_OK;
    }

    /* Création d'un nouveau fichier vide. */
    f = fopen(csv_path, "w");
    if (!f)
        return AUTH_ERR_IO;

    fclose(f);
    return AUTH_OK;
}

AuthStatus auth_list_users(const char *csv_path,
                           AuthUser **out_users,
                           size_t *out_count)
{
    if (!csv_path || !out_users || !out_count)
        return AUTH_ERR_INVALID;

    *out_users = NULL;
    *out_count = 0;

    FILE *f = fopen(csv_path, "r");
    if (!f)
        return AUTH_ERR_IO;

    AuthUser *list = NULL;
    size_t    used = 0;
    size_t    cap  = 0;

    char line[AUTH_MAX_LINE];
    while (fgets(line, sizeof(line), f))
    {
        AuthUser u;
        AuthStatus st = auth_parse_line(line, &u);
        if (st != AUTH_OK)
        {
            fclose(f);
            free(list);
            return AUTH_ERR_FORMAT;
        }

        if (used == cap)
        {
            size_t new_cap = cap == 0 ? 8 : cap * 2;
            AuthUser *tmp = (AuthUser *)realloc(list, new_cap * sizeof(AuthUser));
            if (!tmp)
            {
                fclose(f);
                free(list);
                return AUTH_ERR_IO;
            }
            list = tmp;
            cap  = new_cap;
        }
        list[used++] = u;
    }

    fclose(f);

    *out_users = list;
    *out_count = used;
    return AUTH_OK;
}

void auth_free_user_list(AuthUser *users)
{
    free(users);
}

/**
 * @brief Sauvegarde une liste d'utilisateurs complète dans le CSV (remplacement).
 */
static AuthStatus auth_save_all(const char *csv_path,
                                const AuthUser *users,
                                size_t count)
{
    FILE *f = fopen(csv_path, "w");
    if (!f)
        return AUTH_ERR_IO;

    for (size_t i = 0; i < count; ++i)
    {
        AuthStatus st = auth_fprint_user(f, &users[i]);
        if (st != AUTH_OK)
        {
            fclose(f);
            return st;
        }
    }

    fclose(f);
    return AUTH_OK;
}

AuthStatus auth_register_user(const char *csv_path,
                              const char *username,
                              const char *password,
                              const char *role)
{
    if (!csv_path || !username || !password || !role)
        return AUTH_ERR_INVALID;

    AuthStatus st = auth_init(csv_path);
    if (st != AUTH_OK)
        return st;

    AuthUser *users = NULL;
    size_t    count = 0;

    st = auth_list_users(csv_path, &users, &count);
    if (st != AUTH_OK && st != AUTH_ERR_IO)
    {
        /* AUTH_ERR_IO est déjà géré dans auth_list_users, ici on renvoie st. */
        return st;
    }

    /* Vérifie l'unicité de l'identifiant. */
    for (size_t i = 0; i < count; ++i)
    {
        if (strcmp(users[i].username, username) == 0)
        {
            auth_free_user_list(users);
            return AUTH_ERR_EXISTS;
        }
    }

    /* Ajout du nouvel utilisateur. */
    AuthUser *tmp = (AuthUser *)realloc(users, (count + 1) * sizeof(AuthUser));
    if (!tmp)
    {
        auth_free_user_list(users);
        return AUTH_ERR_IO;
    }
    users = tmp;

    AuthUser *nu = &users[count];
    memset(nu, 0, sizeof(*nu));
    strncpy(nu->username, username, AUTH_MAX_USERNAME);
    nu->username[AUTH_MAX_USERNAME] = '\0';
    strncpy(nu->password, password, AUTH_MAX_PASSWORD);
    nu->password[AUTH_MAX_PASSWORD] = '\0';
    strncpy(nu->role, role, AUTH_MAX_ROLE);
    nu->role[AUTH_MAX_ROLE] = '\0';
    nu->active = 1;

    st = auth_save_all(csv_path, users, count + 1);
    auth_free_user_list(users);
    return st;
}

AuthStatus auth_authenticate(const char *csv_path,
                             const char *username,
                             const char *password,
                             AuthUser *out_user)
{
    if (!csv_path || !username || !password)
        return AUTH_ERR_INVALID;

    AuthUser *users = NULL;
    size_t    count = 0;

    AuthStatus st = auth_list_users(csv_path, &users, &count);
    if (st != AUTH_OK)
        return st;

    for (size_t i = 0; i < count; ++i)
    {
        if (strcmp(users[i].username, username) == 0)
        {
            if (!users[i].active)
            {
                auth_free_user_list(users);
                return AUTH_ERR_INVALID; /* Compte inactif. */
            }
            if (strcmp(users[i].password, password) == 0)
            {
                if (out_user)
                {
                    *out_user = users[i];
                }
                auth_free_user_list(users);
                return AUTH_OK;
            }
            else
            {
                auth_free_user_list(users);
                return AUTH_ERR_INVALID; /* Mauvais mot de passe. */
            }
        }
    }

    auth_free_user_list(users);
    return AUTH_ERR_NOTFOUND;
}

AuthStatus auth_change_password(const char *csv_path,
                                const char *username,
                                const char *old_password,
                                const char *new_password)
{
    if (!csv_path || !username || !new_password)
        return AUTH_ERR_INVALID;

    AuthUser *users = NULL;
    size_t    count = 0;

    AuthStatus st = auth_list_users(csv_path, &users, &count);
    if (st != AUTH_OK)
        return st;

    size_t index = (size_t)-1;
    for (size_t i = 0; i < count; ++i)
    {
        if (strcmp(users[i].username, username) == 0)
        {
            index = i;
            break;
        }
    }

    if (index == (size_t)-1)
    {
        auth_free_user_list(users);
        return AUTH_ERR_NOTFOUND;
    }

    if (old_password)
    {
        if (strcmp(users[index].password, old_password) != 0)
        {
            auth_free_user_list(users);
            return AUTH_ERR_INVALID;
        }
    }

    strncpy(users[index].password, new_password, AUTH_MAX_PASSWORD);
    users[index].password[AUTH_MAX_PASSWORD] = '\0';

    st = auth_save_all(csv_path, users, count);
    auth_free_user_list(users);
    return st;
}

AuthStatus auth_set_active(const char *csv_path,
                           const char *username,
                           int active)
{
    if (!csv_path || !username)
        return AUTH_ERR_INVALID;

    AuthUser *users = NULL;
    size_t    count = 0;

    AuthStatus st = auth_list_users(csv_path, &users, &count);
    if (st != AUTH_OK)
        return st;

    size_t index = (size_t)-1;
    for (size_t i = 0; i < count; ++i)
    {
        if (strcmp(users[i].username, username) == 0)
        {
            index = i;
            break;
        }
    }

    if (index == (size_t)-1)
    {
        auth_free_user_list(users);
        return AUTH_ERR_NOTFOUND;
    }

    users[index].active = active ? 1 : 0;

    st = auth_save_all(csv_path, users, count);
    auth_free_user_list(users);
    return st;
}


#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "common.h"

#define MAX_QUERY_LENGTH 1024

typedef struct {
    char question[MAX_QUESTION_LENGTH];
    SupportType support_type;
    char *support;
    int number_of_valid_answers;
    char **valid_answers;
} Question;

// Helper to trim leading/trailing spaces
char *__trim(char *str) {
    while (isspace((unsigned char)*str)) str++; // Leading
    if (*str == 0) return strdup("");           // Empty

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--; // Trailing
    *(end + 1) = '\0';

    return strdup(str);
}

char **__parse_string(char *string, size_t *number_of_valid_answers) {
    if (!string) return NULL;

    // Estimate max number of tokens (worst case: every char is a token)
    size_t max_tokens = 1;
    for (char *p = string; *p; ++p) {
        if (*p == ',') max_tokens++;
    }

    // Allocate array for pointers + null-terminator
    char **result = malloc((max_tokens + 1) * sizeof(char *));
    if (!result) return NULL;

    size_t idx = 0;
    char *token = strtok(string, ",");
    while (token != NULL) {
        result[idx++] = __trim(token); // Trim and store
        token = strtok(NULL, ",");
    }

    result[idx] = NULL; // Null-terminate the array
    *number_of_valid_answers = idx;
    return result;
}

// Convert accented Latin characters to ASCII equivalents
char __replace_accent(unsigned char c) {
    switch (c) {
        // Lowercase
        case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return 'a'; // à á â ã ä å
        case 0xE8: case 0xE9: case 0xEA: case 0xEB: return 'e';                     // è é ê ë
        case 0xEC: case 0xED: case 0xEE: case 0xEF: return 'i';                     // ì í î ï
        case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: return 'o';          // ò ó ô õ ö
        case 0xF9: case 0xFA: case 0xFB: case 0xFC: return 'u';                     // ù ú û ü
        case 0xE7: return 'c';                                                     // ç
        case 0xF1: return 'n';                                                     // ñ
        case 0xFD: case 0xFF: return 'y';                                          // ý ÿ

        // Uppercase
        case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: return 'A'; // À Á Â Ã Ä Å
        case 0xC8: case 0xC9: case 0xCA: case 0xCB: return 'E';                       // È É Ê Ë
        case 0xCC: case 0xCD: case 0xCE: case 0xCF: return 'I';                       // Ì Í Î Ï
        case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: return 'O';            // Ò Ó Ô Õ Ö
        case 0xD9: case 0xDA: case 0xDB: case 0xDC: return 'U';                       // Ù Ú Û Ü
        case 0xC7: return 'C';                                                       // Ç
        case 0xD1: return 'N';                                                       // Ñ
        case 0xDD: return 'Y';  

        default: return c;
    }
}

// Sanitize a string: lowercase, strip non-alphanumeric, convert accents
char *__sanitize_token(const char *input) {
    size_t len = strlen(input);
    char *output = malloc(len + 1); // Max possible length
    if (!output) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)input[i];
        c = __replace_accent(c);

        if (isalnum(c)) {
            output[j++] = tolower(c);
        }
    }

    output[j] = '\0';
    return output;
}

char *__combine_strings(char **array, size_t size) {
    if (!array || size == 0) return NULL;

    // First pass: sanitize and calculate total length
    char **cleaned = malloc(size * sizeof(char *));
    if (!cleaned) return NULL;

    size_t total_length = 0;
    for (size_t i = 0; i < size; i++) {
        cleaned[i] = __sanitize_token(array[i]);
        if (!cleaned[i]) {
            // Free everything on failure
            for (size_t k = 0; k < i; k++) free(cleaned[k]);
            free(cleaned);
            return NULL;
        }
        total_length += strlen(cleaned[i]);
    }

    total_length += size - 1; // For commas
    char *result = malloc(total_length + 1);
    if (!result) {
        for (size_t i = 0; i < size; i++) free(cleaned[i]);
        free(cleaned);
        return NULL;
    }

    // Build final string
    result[0] = '\0';
    for (size_t i = 0; i < size; i++) {
        strcat(result, cleaned[i]);
        if (i < size - 1) strcat(result, ",");
        free(cleaned[i]);
    }
    free(cleaned);

    return result;
}

// Creates the database and the 'questions' table
int init_database(const char *db_filename) {
    sqlite3 *db;
    char *err_msg = NULL;

    const char *sql = 
        "CREATE TABLE IF NOT EXISTS questions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "question TEXT NOT NULL,"
        "support_type INTEGER NOT NULL,"
        "support TEXT,"
        "valid_answers TEXT"
        ");";

    int rc = sqlite3_open(db_filename, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return rc;
    }

    sqlite3_close(db);
    return SQLITE_OK;
}

// Creates a table named lobby_<id> with a foreign key to questions(id)
int create_lobby_table(const char *db_filename, int lobby_id) {
    sqlite3 *db;
    char *err_msg = NULL;

    // Build the SQL query
    char sql[256];
    snprintf(sql, sizeof(sql),
        "CREATE TABLE IF NOT EXISTS lobby_%d ("
        "question_id INTEGER,"
        "FOREIGN KEY(question_id) REFERENCES questions(id)"
        ");", lobby_id);

    // Open the SQLite database
    int rc = sqlite3_open(db_filename, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    // Enable foreign key enforcement
    rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to enable foreign keys: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return rc;
    }

    // Execute the CREATE TABLE statement
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return rc;
    }

    sqlite3_close(db);
    return SQLITE_OK;
}

int wipe_lobby_table(const char *db_filename, int lobby_id) {
    sqlite3 *db;
    char *err_msg = NULL;

    // Build the SQL DELETE statement
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM lobby_%d;", lobby_id);

    // Open database
    int rc = sqlite3_open(db_filename, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open DB: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    // Execute delete statement
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error (wipe): %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return rc;
    }

    sqlite3_close(db);
    return SQLITE_OK;
}

int destroy_lobby_table(const char *db_filename, int lobby_id) {
    sqlite3 *db;
    char *err_msg = NULL;

    // Build the SQL DROP TABLE statement
    char sql[256];
    snprintf(sql, sizeof(sql), "DROP TABLE IF EXISTS lobby_%d;", lobby_id);

    // Open database
    int rc = sqlite3_open(db_filename, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open DB: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    // Execute drop statement
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error (drop): %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return rc;
    }

    sqlite3_close(db);
    return SQLITE_OK;
}

int get_random_questions(const char *db_filename, int lobby_id, int n, Question **out_questions) {
    sqlite3 *db;
    
    // Build the SQL DROP TABLE statement
    char sql[256];
    snprintf(sql, sizeof(sql), "DROP TABLE IF EXISTS lobby_%d;", lobby_id);

    // Open database
    int rc = sqlite3_open(db_filename, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open DB: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    char query[MAX_QUERY_LENGTH];
    sqlite3_stmt *stmt = NULL;

    snprintf(query, sizeof(query),
        "SELECT id, question, support_type, support, valid_answers FROM questions "
        "WHERE id NOT IN (SELECT question_id FROM lobby_%d) "
        "ORDER BY RANDOM() LIMIT ?;", lobby_id);

    rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error in prepare: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, n);

    Question *questions = malloc(sizeof(Question) * n);
    if (!questions) {
        fprintf(stderr, "Memory allocation failed\n");
        sqlite3_finalize(stmt);
        return -1;
    }

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *qtext = sqlite3_column_text(stmt, 1);
        int support_type = sqlite3_column_int(stmt, 2);
        const unsigned char *support = sqlite3_column_text(stmt, 3);
        const unsigned char *valid_answers_str = sqlite3_column_text(stmt, 4);

        size_t number_of_valid_answers;
        strncpy(questions[count].question, (const char *)qtext, MAX_QUESTION_LENGTH - 1);
        questions[count].question[MAX_QUESTION_LENGTH - 1] = '\0'; // Ensure null-termination
        questions[count].support_type = (SupportType)support_type;
        questions[count].support = support ? strdup((const char *)support) : NULL;
        questions[count].valid_answers = __parse_string((char *)valid_answers_str, &number_of_valid_answers);
        questions[count].number_of_valid_answers = number_of_valid_answers;

        // Insert this question ID into the lobby table
        char insert_query[128];
        snprintf(insert_query, sizeof(insert_query),
                 "INSERT INTO lobby_%d (question_id) VALUES (?);", lobby_id);
        sqlite3_stmt *insert_stmt;
        if (sqlite3_prepare_v2(db, insert_query, -1, &insert_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(insert_stmt, 1, id);
            if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
                fprintf(stderr, "Insert failed: %s\n", sqlite3_errmsg(db));
            }
            sqlite3_finalize(insert_stmt);
        } else {
            fprintf(stderr, "Prepare insert failed: %s\n", sqlite3_errmsg(db));
        }

        count++;
    }

    sqlite3_finalize(stmt);
    *out_questions = questions;

    return count; // Return how many questions were actually retrieved
}

int insert_question(sqlite3 *db, Question *q, size_t answer_count) {
    const char *sql = "INSERT INTO questions (question, support_type, support, valid_answers) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    int rc;

    // Combine valid_answers array into a single string
    char *valid_answers_combined = __combine_strings(q->valid_answers, answer_count);
    if (!valid_answers_combined) {
        fprintf(stderr, "Failed to combine valid answers\n");
        return -1;
    }

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
        free(valid_answers_combined);
        return -1;
    }

    // Bind the parameters
    sqlite3_bind_text(stmt, 1, q->question, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, (int)q->support_type);
    if (q->support) {
        sqlite3_bind_text(stmt, 3, q->support, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    sqlite3_bind_text(stmt, 4, valid_answers_combined, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "SQL insert error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        free(valid_answers_combined);
        return -1;
    }

    sqlite3_finalize(stmt);
    free(valid_answers_combined);

    return 0; // success
}
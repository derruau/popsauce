#ifndef QUESTIONS_HEADER
#define QUESTIONS_HEADER

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
char *__trim(char *str);

// Parses a comma-separated string into an array of strings
char **__parse_string(char *string, size_t *number_of_valid_answers);

// Map known Unicode codepoints to ASCII
char __unicode_to_ascii(int codepoint);

// Converts a UTF-8 string to ASCII by replacing accented Latin-1 characters
// Returns a newly allocated string. Caller must free it.
char *__utf8_to_ascii(const char *input);

// Sanitize a string: lowercase, strip non-alphanumeric, convert accents
char *__sanitize_token(const char *input);

// Combines an array of strings into a single string, separated by commas.
char *__combine_strings(char **array, size_t size);

// Creates the database and the 'questions' table
int init_database(const char *db_filename);

// Creates a table named lobby_<id> with a foreign key to questions(id)
int create_lobby_table(const char *db_filename, int lobby_id);

// Wipes the lobby table by deleting all entries
int wipe_lobby_table(const char *db_filename, int lobby_id);

// Destroys the lobby table by dropping it
int destroy_lobby_table(const char *db_filename, int lobby_id);

// Retrieves 'n' random questions from the database that have not yet been asked in this specific lobby.
int get_random_questions(const char *db_filename, int lobby_id, int n, Question **out_questions);

// Inserts a question into the database.
int insert_question(const char *db_filename, Question *q, size_t answer_count);

#endif
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

char **__parse_string(char *string, size_t *number_of_valid_answers);

// Convert accented Latin characters to ASCII equivalents
char __replace_accent(unsigned char c);

// Sanitize a string: lowercase, strip non-alphanumeric, convert accents
char *__sanitize_token(const char *input);

char *__combine_strings(char **array, size_t size);

// Creates the database and the 'questions' table
int init_database(const char *db_filename);

// Creates a table named lobby_<id> with a foreign key to questions(id)
int create_lobby_table(const char *db_filename, int lobby_id);

int wipe_lobby_table(const char *db_filename, int lobby_id);

int destroy_lobby_table(const char *db_filename, int lobby_id);

int get_random_questions(const char *db_filename, int lobby_id, int n, Question **out_questions);

int insert_question(const char *db_filename, Question *q, size_t answer_count);

#endif
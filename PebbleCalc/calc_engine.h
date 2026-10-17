#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  CALC_OK = 0,
  CALC_ERR_EMPTY,
  CALC_ERR_SYNTAX,
  CALC_ERR_DIV_ZERO,
  CALC_ERR_OVERFLOW
} CalcError;

CalcError calc_evaluate(const char *expr, double *out_result);

void calc_format_result(double val, char *buf, size_t buf_size);

void calc_format_currency(char *buf, size_t buf_size, double val);

const char* calc_get_smart_bracket(const char *expr, int cursor_pos);

bool calc_insert_str(char *buf, int *cursor_pos, size_t max_buf_size, const char *str);

bool calc_backspace(char *buf, int *cursor_pos);

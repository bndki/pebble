#include "calc_engine.h"
#include <pebble.h>
#include <stdio.h>
#include <string.h>

static inline double my_fabs(double x) {
  return (x < 0.0) ? -x : x;
}

typedef struct {
  const char *src;
  size_t pos;
  CalcError err;
} CalcParser;

static void skip_whitespace(CalcParser *p) {
  while (p->src[p->pos] && (p->src[p->pos] == ' ' || p->src[p->pos] == '\t')) {
    p->pos++;
  }
}

static char peek(CalcParser *p) {
  skip_whitespace(p);
  return p->src[p->pos];
}

static char get(CalcParser *p) {
  skip_whitespace(p);
  return p->src[p->pos++];
}

static double parse_number_token(const char *str, const char **out_end) {
  double int_part = 0.0;
  double frac_part = 0.0;
  double divisor = 1.0;
  bool has_digits = false;
  const char *p = str;

  while (*p >= '0' && *p <= '9') {
    int_part = int_part * 10.0 + (*p - '0');
    p++;
    has_digits = true;
  }

  if (*p == '.') {
    p++;
    while (*p >= '0' && *p <= '9') {
      frac_part = frac_part * 10.0 + (*p - '0');
      divisor *= 10.0;
      p++;
      has_digits = true;
    }
  }

  if (!has_digits) {
    if (out_end) *out_end = str;
    return 0.0;
  }

  if (out_end) *out_end = p;
  return int_part + (frac_part / divisor);
}

static double parse_expression(CalcParser *p);

static double parse_primary(CalcParser *p) {
  skip_whitespace(p);
  char c = peek(p);

  if (c == '(') {
    get(p);
    double val = parse_expression(p);
    if (p->err != CALC_OK) return 0.0;
    if (peek(p) == ')') {
      get(p);
    }
    return val;
  }

  if ((c >= '0' && c <= '9') || c == '.') {
    const char *start = &p->src[p->pos];
    const char *end = NULL;
    double val = parse_number_token(start, &end);
    if (end == start) {
      p->err = CALC_ERR_SYNTAX;
      return 0.0;
    }
    p->pos += (end - start);
    return val;
  }

  p->err = CALC_ERR_SYNTAX;
  return 0.0;
}

static double parse_factor(CalcParser *p) {
  skip_whitespace(p);
  char c = peek(p);
  if (c == '+') {
    get(p);
    return parse_factor(p);
  } else if (c == '-') {
    get(p);
    return -parse_factor(p);
  }
  return parse_primary(p);
}

static double parse_term(CalcParser *p) {
  double left = parse_factor(p);
  if (p->err != CALC_OK) return 0.0;

  while (1) {
    char c = peek(p);
    if (c == '*' || c == 'x' || c == 'X') {
      get(p);
      double right = parse_factor(p);
      if (p->err != CALC_OK) return 0.0;
      left *= right;
    } else if (c == '/') {
      get(p);
      double right = parse_factor(p);
      if (p->err != CALC_OK) return 0.0;
      if (my_fabs(right) < 1e-12) {
        p->err = CALC_ERR_DIV_ZERO;
        return 0.0;
      }
      left /= right;
    } else {
      break;
    }
  }
  return left;
}

static double parse_expression(CalcParser *p) {
  double left = parse_term(p);
  if (p->err != CALC_OK) return 0.0;

  while (1) {
    char c = peek(p);
    if (c == '+') {
      get(p);
      double right = parse_term(p);
      if (p->err != CALC_OK) return 0.0;
      left += right;
    } else if (c == '-') {
      get(p);
      double right = parse_term(p);
      if (p->err != CALC_OK) return 0.0;
      left -= right;
    } else {
      break;
    }
  }
  return left;
}

CalcError calc_evaluate(const char *expr, double *out_result) {
  if (!expr || *expr == '\0') {
    if (out_result) *out_result = 0.0;
    return CALC_ERR_EMPTY;
  }

  CalcParser p;
  p.src = expr;
  p.pos = 0;
  p.err = CALC_OK;

  double res = parse_expression(&p);
  skip_whitespace(&p);

  if (p.src[p.pos] != '\0' && p.src[p.pos] != ')') {
    p.err = CALC_ERR_SYNTAX;
  }

  if (p.err != CALC_OK) {
    return p.err;
  }

  if (out_result) {
    *out_result = res;
  }
  return CALC_OK;
}

void calc_format_currency(char *buf, size_t buf_size, double val) {
  if (!buf || buf_size == 0) return;
  if (val < 0.0) val = 0.0;
  long cents_total = (long)(val * 100.0 + 0.5);
  long dollars = cents_total / 100;
  long cents = cents_total % 100;
  snprintf(buf, buf_size, "$%ld.%02ld", dollars, cents);
}

void calc_format_result(double val, char *buf, size_t buf_size) {
  if (!buf || buf_size == 0) return;

  bool is_neg = false;
  if (val < 0.0) {
    is_neg = true;
    val = -val;
  }

  long int_part = (long)val;
  double frac = val - (double)int_part;
  long frac_part = (long)(frac * 1000000.0 + 0.5);

  if (frac_part >= 1000000) {
    int_part++;
    frac_part -= 1000000;
  }

  if (frac_part == 0) {
    snprintf(buf, buf_size, "%s%ld", is_neg ? "-" : "", int_part);
  } else {
    char frac_buf[32];
    snprintf(frac_buf, sizeof(frac_buf), "%06ld", frac_part);
    int end = (int)strlen(frac_buf) - 1;
    while (end >= 0 && frac_buf[end] == '0') {
      frac_buf[end] = '\0';
      end--;
    }
    if (end >= 0) {
      snprintf(buf, buf_size, "%s%ld.%s", is_neg ? "-" : "", int_part, frac_buf);
    } else {
      snprintf(buf, buf_size, "%s%ld", is_neg ? "-" : "", int_part);
    }
  }
}

const char* calc_get_smart_bracket(const char *expr, int cursor_pos) {
  if (!expr || cursor_pos < 0) {
    return "(";
  }

  int len = (int)strlen(expr);
  if (cursor_pos > len) {
    cursor_pos = len;
  }

  int open_count = 0;
  for (int i = 0; i < cursor_pos; i++) {
    if (expr[i] == '(') {
      open_count++;
    } else if (expr[i] == ')') {
      if (open_count > 0) {
        open_count--;
      }
    }
  }

  char prev = '\0';
  for (int i = cursor_pos - 1; i >= 0; i--) {
    if (expr[i] != ' ' && expr[i] != '\t') {
      prev = expr[i];
      break;
    }
  }

  if (prev == '\0') {
    return "(";
  }

  if (prev == '+' || prev == '-' || prev == 'x' || prev == 'X' || prev == '*' || prev == '/' || prev == '(') {
    return "(";
  }

  if ((prev >= '0' && prev <= '9') || prev == '.' || prev == ')') {
    if (open_count > 0) {
      return ")";
    } else {
      return "x(";
    }
  }

  return "(";
}

bool calc_insert_str(char *buf, int *cursor_pos, size_t max_buf_size, const char *str) {
  if (!buf || !cursor_pos || !str) return false;

  size_t cur_len = strlen(buf);
  size_t ins_len = strlen(str);

  if (cur_len + ins_len >= max_buf_size) {
    return false;
  }

  int pos = *cursor_pos;
  if (pos < 0) pos = 0;
  if ((size_t)pos > cur_len) pos = (int)cur_len;

  memmove(&buf[pos + ins_len], &buf[pos], cur_len - pos + 1);
  memcpy(&buf[pos], str, ins_len);

  *cursor_pos = pos + (int)ins_len;
  return true;
}

bool calc_backspace(char *buf, int *cursor_pos) {
  if (!buf || !cursor_pos || *cursor_pos <= 0) return false;

  size_t cur_len = strlen(buf);
  int pos = *cursor_pos;
  if (pos > (int)cur_len) pos = (int)cur_len;

  memmove(&buf[pos - 1], &buf[pos], cur_len - pos + 1);

  *cursor_pos = pos - 1;
  return true;
}


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include <vlib/ini.h>

void ini_init(INI* self) {
  self->root = NULL;
}
void ini_close(INI* self) {
  INI_Node* tmp;
  for (INI_Node* section = self->root; section; section = tmp) {
    for (INI_Node* entry = section->data; entry; entry = tmp) {
      tmp = entry->next;
      free(entry->name);
      free(entry->data);
      free(entry);
    }
    tmp = section->next;
    free(section->name);
    free(section);
  }
}

void ini_clear(INI* self) {
  ini_close(self);
  ini_init(self);
}

static INI_Node* find_node(INI_Node** ptr, const char* name, int create) {
  INI_Node* node = *ptr;
  while (node && strcmp(node->name, name) != 0) {
    ptr = &node->next;
    node = *ptr;
  }
  if (!node && create) {
    node = malloc(sizeof(INI_Node));
    node->next = NULL;
    node->name = strdup(name);
    node->data = NULL;
    *ptr = node;
  }
  return node;
}

const char* ini_lookup(INI* self, const char* section, const char* key) {
  INI_Node* snode = find_node(&self->root, section, 0);
  if (!snode) return NULL;
  INI_Node* dnode = find_node((INI_Node**)&snode->data, key, 0);
  return dnode ? dnode->data : NULL;
}

void ini_add(INI* self, const char* section, const char* key, const char* value) {
  INI_Node* snode = find_node(&self->root, section, 1);
  INI_Node* dnode = find_node((INI_Node**)&snode->data, key, 1);
  if (dnode->data) {
    free(dnode->data);
  }
  dnode->data = strdup(value);
}

/* Parsing */

typedef struct Parser {
  INI*        ini;
  char*       section;
  const char* error;
  char        buf[200];
} Parser;

static char* trim(const char* src, size_t len) {
  unsigned start = 0;
  while (start < len && isspace(src[start])) start++;
  len -= start;
  src += start;
  while (len > 0 && isspace(src[len-1])) len--;

  char* result = malloc(len+1);
  memcpy(result, src, len);
  result[len] = 0;
  return result;
}

static int parse_section(Parser* self, const char* line, size_t len) {
  unsigned i = 0;

  // Fill buffer with characters until a ']' is reached
  while (i < len && line[i] != ']') {
    if (i+1 == sizeof(self->buf)) {
      self->error = "section name is too long";
      return -1;
    }
    self->buf[i] = line[i];
    i++;
  }

  if (i == len || line[i] != ']') {
    self->error = "unmatched ']'";
    return -1;
  }
  self->buf[i] = 0;
  i++;

  // Expect whitespace until the end of the line
  while (i < len) {
    if (!isspace(line[i])) {
      self->error = "unexpected character after section header";
      return -1;
    }
    i++;
  }

  if (self->section) free(self->section);
  self->section = strdup(self->buf);
  return 0;
}
static int parse_entry(Parser* self, const char* line, size_t len) {
  unsigned i = 0;
  unsigned bufi = 0;

  // Read key
  while (i < len && line[i] != '=') {
    if (bufi+1 == sizeof(self->buf)) {
      self->error = "key name is too long";
      return -1;
    }
    self->buf[bufi++] = line[i++];
  }
  if (i == len) {
    self->error = "'=' not found";
    return -1;
  }
  i++; // consume '='

  char* key = trim(self->buf, bufi);
  bufi = 0;

  // Read value
  bufi = 0;
  while (i < len && isspace(line[i])) i++;
  while (i < len) {
    if (bufi+1 == sizeof(self->buf)) {
      self->error = "value is too long";
      free(key);
      return -1;
    }
    self->buf[bufi++] = line[i++];
  }

  char* value = trim(self->buf, bufi);

  // Add entry
  ini_add(self->ini, self->section, key, value);
  free(key);
  free(value);
  return 0;
}
static int parse_line(Parser* self, const char* line, size_t len) {
  unsigned i = 0;

  // Skip whitespace
  while (i < len && isspace(line[i])) i++;
  
  if (i == len) {
    // Blank line
    return 0;
  }

  switch (line[i]) {

    case ';':
    case '#':
      // Comment
      return 0;

    case '[':
      i++;
      return parse_section(self, line+i, len-i);

    default:
      if (!self->section) {
        self->error = "entry before section header";
        return -1;
      }
      return parse_entry(self, line, len);

  }
}

int ini_parse(INI* self, const char* text, const char** err) {
  assert(err);

  Parser parser = {
    .ini = self,
  };

  unsigned i = 0;
  unsigned line = 1;
  for (;;) {
    if (text[i] == '\n' || text[i] == 0) {
      int r = parse_line(&parser, text, i);
      if (r) goto error;
      if (text[i] == 0) break;
      text += i+1;
      i = 0;
      line++;
    } else {
      i++;
    }
  }

  if (parser.section) free(parser.section);
  return 0;

error:
  snprintf(parser.buf, sizeof(parser.buf), "line %u: %s", line, parser.error);
  *err = parser.buf;
  if (parser.section) free(parser.section);
  return -1;
}

void ini_dump(INI* self, char* buf, size_t bufsz) {
  unsigned i = 0;

  for (INI_Node* section = self->root; section; section = section->next) {
    i += snprintf(buf+i, bufsz-i, "[%s]\n", section->name);
    if (i+1 >= bufsz) return;
    for (INI_Node* entry = section->data; entry; entry = entry->next) {
      i += snprintf(buf+i, bufsz-i, "%s=%s\n", entry->name, (char*)entry->data);
      if (i+1 >= bufsz) return;
    }
  }

}

/* GQI */

typedef struct GQI_INI {
  GQI     _base;
  INI*    ini;
  bool    owns;
} GQI_INI;

static int gqi_ini_query(void* _self, GQI_String* input, GQI_String* result) {
  GQI_INI* self = _self;

  char *section = NULL, *key = NULL;
  char buf[200];

  unsigned bufi = 0;
  unsigned i = 0;
  for (; i < input->sz && input->str[i] != '/'; i++) {
    if (bufi+1 == sizeof(buf)) goto nope;
    buf[bufi++] = input->str[i];
  }
  i++;
  buf[bufi] = 0;
  section = strdup(buf);

  bufi = 0;
  for (; i < input->sz; i++) {
    if (bufi+1 == sizeof(buf)) goto nope;
    buf[bufi++] = input->str[i];
  }
  buf[bufi] = 0;
  key = strdup(buf);

  const char* value = ini_lookup(self->ini, section, key);
  free(section);
  free(key);
  if (value) {
    gqis_init_copy(result, value, strlen(value));
    return 0;
  } else {
    gqis_init_null(result);
    return 0;
  }

nope:
  if (section) free(section);
  if (key) free(key);
  const char* err = "INI query string too long";
  gqis_init_static(result, err, strlen(err));
  return -1;
}
static void gqi_ini_close(void* _self) {
  GQI_INI* self = _self;
  if (self->owns) {
    ini_close(self->ini);
    free(self->ini);
  }
  free(self);
}

static GQI_Impl gqi_ini_impl = {
  .query = gqi_ini_query,
  .close = gqi_ini_close,
};

GQI* gqi_new_ini(INI* ini, bool free_on_close) {
  GQI_INI* self = malloc(sizeof(GQI_INI));
  gqi_init(self, &gqi_ini_impl);
  self->ini = ini;
  self->owns = (free_on_close != 0);
  return (GQI*)self;
}

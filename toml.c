/*
 MIT License

 Copyright (c) 2017 CK Tan
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "toml.h"

typedef struct node_t node_t;
struct node_t
{
  int type; /* 1: пары ключ-значение, 2: таблицы, 3: массивы */
  char *key;
  void *value;
};

struct toml_table_t
{
  char *key;
  node_t **nodes;
  int node_count;
  int node_cap;
};

struct toml_array_t
{
  int type; /* 's':строка, 't':таблица, 'a':массив, 'v':скаляр */
  void **values;
  int nelem;
  int cap;
};

static void* xmalloc( size_t sz )
{
  void *p = malloc( sz );
  if ( !p && sz > 0 )
    abort();
  return p;
}

static void* xrealloc( void *ptr, size_t sz )
{
  void *p = realloc( ptr, sz );
  if ( !p && sz > 0 )
    abort();
  return p;
}

static void free_table( toml_table_t *tab );
static void free_array( toml_array_t *arr );

static void free_node( node_t *node )
{
  if ( !node )
    return;
  free( node->key );
  if ( node->type == 1 )
    free( node->value );
  else if ( node->type == 2 )
    free_table( (toml_table_t*) node->value );
  else if ( node->type == 3 )
    free_array( (toml_array_t*) node->value );
  free( node );
}

static void free_table( toml_table_t *tab )
{
  if ( !tab )
    return;
  free( tab->key );
  for ( int i = 0; i < tab->node_count; i++ )
    free_node( tab->nodes[i] );
  free( tab->nodes );
  free( tab );
}

static void free_array( toml_array_t *arr )
{
  if ( !arr )
    return;
  if ( arr->type == 't' )
  {
    for ( int i = 0; i < arr->nelem; i++ )
      free_table( (toml_table_t*) arr->values[i] );
  }
  else if ( arr->type == 'a' )
  {
    for ( int i = 0; i < arr->nelem; i++ )
      free_array( (toml_array_t*) arr->values[i] );
  }
  else
  {
    for ( int i = 0; i < arr->nelem; i++ )
      free( arr->values[i] );
  }
  free( arr->values );
  free( arr );
}

void toml_free( toml_table_t *tab )
{
  free_table( tab );
}

static node_t* find_node( const toml_table_t *tab, const char *key, int type )
{
  if ( !tab )
    return NULL;
  for ( int i = 0; i < tab->node_count; i++ )
  {
    if ( strcmp( tab->nodes[i]->key, key ) == 0
        && (type == 0 || tab->nodes[i]->type == type) )
    {
      return tab->nodes[i];
    }
  }
  return NULL;
}

const char* toml_key_in( const toml_table_t *tab, int keyidx )
{
  if ( !tab || keyidx < 0 || keyidx >= tab->node_count )
    return NULL;
  return tab->nodes[keyidx]->key;
}

toml_table_t* toml_table_in( const toml_table_t *tab, const char *key )
{
  node_t *node = find_node( tab, key, 2 );
  return node ? (toml_table_t*) node->value : NULL;
}

toml_array_t* toml_array_in( const toml_table_t *tab, const char *key )
{
  node_t *node = find_node( tab, key, 3 );
  return node ? (toml_array_t*) node->value : NULL;
}

int toml_array_nelem( const toml_array_t *arr )
{
  return arr ? arr->nelem : 0;
}

toml_table_t* toml_table_at( const toml_array_t *arr, int idx )
{
  if ( !arr || idx < 0 || idx >= arr->nelem || arr->type != 't' )
    return NULL;
  return (toml_table_t*) arr->values[idx];
}

toml_array_t* toml_array_at( const toml_array_t *arr, int idx )
{
  if ( !arr || idx < 0 || idx >= arr->nelem || arr->type != 'a' )
    return NULL;
  return (toml_array_t*) arr->values[idx];
}

static toml_datum_t make_datum( char *val, bool ok )
{
  toml_datum_t d;
  d.ok = ok;
  d.u.s = val;
  return d;
}

toml_datum_t toml_string_in( const toml_table_t *tab, const char *key )
{
  node_t *node = find_node( tab, key, 1 );
  if ( !node )
    return make_datum( NULL, false );
  return make_datum( strdup( (char*) node->value ), true );
}

toml_datum_t toml_bool_in( const toml_table_t *tab, const char *key )
{
  node_t *node = find_node( tab, key, 1 );
  toml_datum_t d =
    { 0 };
  if ( !node )
    return d;
  char *s = (char*) node->value;
  if ( strcmp( s, "true" ) == 0 )
  {
    d.ok = true;
    d.u.b = true;
  }
  else if ( strcmp( s, "false" ) == 0 )
  {
    d.ok = true;
    d.u.b = false;
  }
  return d;
}

toml_datum_t toml_int_in( const toml_table_t *tab, const char *key )
{
  node_t *node = find_node( tab, key, 1 );
  toml_datum_t d =
    { 0 };
  if ( !node )
    return d;
  char *s = (char*) node->value;
  char *end;
  long long v = strtoll( s, &end, 0 );
  if ( *end == '\0' )
  {
    d.ok = true;
    d.u.i = (int64_t) v;
  }
  return d;
}

toml_datum_t toml_double_in( const toml_table_t *tab, const char *key )
{
  node_t *node = find_node( tab, key, 1 );
  toml_datum_t d =
    { 0 };
  if ( !node )
    return d;
  char *s = (char*) node->value;
  char *end;
  double v = strtod( s, &end );
  if ( *end == '\0' )
  {
    d.ok = true;
    d.u.d = v;
  }
  return d;
}

toml_datum_t toml_array_at_double( const toml_array_t *arr, int idx )
{
  toml_datum_t d =
    { 0 };
  if ( !arr || idx < 0 || idx >= arr->nelem || arr->type != 'v' )
    return d;
  char *s = (char*) arr->values[idx];
  char *end;
  double v = strtod( s, &end );
  if ( *end == '\0' )
  {
    d.ok = true;
    d.u.d = v;
  }
  return d;
}

/* 
 Парсер строк и базовых токенов TOML
 (Компактный парсер потока файлов без динамической рекурсии кучи)
 */
toml_table_t* toml_parse_file( FILE *fp, char *errbuf, int errbufsz )
{
  if ( !fp )
  {
    snprintf( errbuf, errbufsz, "null file" );
    return NULL;
  }

  // Создаем корневую таблицу контекста сценария
  toml_table_t *root = xmalloc( sizeof(toml_table_t) );
  memset( root, 0, sizeof(toml_table_t) );
  root->key = strdup( "root" );

  char line[1024];
  toml_table_t *cur_tab = root;

  while ( fgets( line, sizeof(line), fp ) )
  {
    char *p = line;
    while ( isspace( (unsigned char )*p ) )
      p++;
    if ( *p == '\0' || *p == '#' )
      continue; // Пропуск пустых строк и русских комментариев

    // Обработка заголовков секций вида [блок] или [[массив]]
    if ( *p == '[' )
    {
      p++;
      bool is_array = false;
      if ( *p == '[' )
      {
        is_array = true;
        p++;
      }
      char *start = p;
      while ( *p && *p != ']' )
        p++;
      *p = '\0';

      // Выделение новой таблицы под-сценария аналитика
      toml_table_t *new_tab = xmalloc( sizeof(toml_table_t) );
      memset( new_tab, 0, sizeof(toml_table_t) );
      new_tab->key = strdup( start );

      if ( is_array )
      {
        // Если встретили [[массив]], укладываем в массив таблиц корневого кортура
        toml_array_t *arr = toml_array_in( root, start );
        if ( !arr )
        {
          arr = xmalloc( sizeof(toml_array_t) );
          memset( arr, 0, sizeof(toml_array_t) );
          arr->type = 't';

          node_t *n = xmalloc( sizeof(node_t) );
          n->type = 3;
          n->key = strdup( start );
          n->value = arr;

          if ( root->node_count >= root->node_cap )
          {
            root->node_cap = root->node_cap == 0 ? 4 : root->node_cap * 2;
            root->nodes = xrealloc( root->nodes,
                root->node_cap * sizeof(node_t*) );
          }
          root->nodes[root->node_count++] = n;
        }
        if ( arr->nelem >= arr->cap )
        {
          arr->cap = arr->cap == 0 ? 4 : arr->cap * 2;
          arr->values = xrealloc( arr->values, arr->cap * sizeof(void*) );
        }
        arr->values[arr->nelem++] = new_tab;
      }
      else
      {
        node_t *n = xmalloc( sizeof(node_t) );
        n->type = 2;
        n->key = strdup( start );
        n->value = new_tab;

        if ( root->node_count >= root->node_cap )
        {
          root->node_cap = root->node_cap == 0 ? 4 : root->node_cap * 2;
          root->nodes = xrealloc( root->nodes,
              root->node_cap * sizeof(node_t*) );
        }
        root->nodes[root->node_count++] = n;
      }
      cur_tab = new_tab;
      continue;
    }

    // Выделение пар парамеров вида: ключ = значение
    char *eq = strchr( p, '=' );
    if ( eq )
    {
      *eq = '\0';
      char *k = p;
      char *v = eq + 1;

      // Очистка от пробелов краев строк UTF-8 ключей
      while ( *k && isspace( (unsigned char )*k ) )
        k++;
      char *kend = k + strlen( k ) - 1;
      while ( kend > k && isspace( (unsigned char )*kend ) )
      {
        *kend = '\0';
        kend--;
      }

      while ( *v && isspace( (unsigned char )*v ) )
        v++;
      char *vend = v + strlen( v ) - 1;
      while ( vend > v && isspace( (unsigned char )*vend ) )
      {
        *vend = '\0';
        vend--;
      }

      // Если значение обернуто кавычками — очищаем
      if ( *v == '"' )
      {
        v++;
        char *qe = strrchr( v, '"' );
        if ( qe )
          *qe = '\0';
      }

      // Логика парсинга встроенного линейного 3D-вектора вида [x, y, z]
      if (*v == '[') {
          v++;
          char* ve = strchr(v, ']');
          if (ve) *ve = '\0';

          toml_array_t* arr = xmalloc(sizeof(toml_array_t));
          memset(arr, 0, sizeof(toml_array_t));
          arr->type = 'v';

          char* token = strtok(v, ",");
          while (token) {
              // Исправление: передаем символ *token, а не сам указатель в isspace
              while (*token && isspace((unsigned char)*token)) token++;

              // Исправление: tk_end обязан быть указателем char*, а не символом char
              char* tk_end = token + strlen(token) - 1;
              while (tk_end > token && isspace((unsigned char)*tk_end)) {
                  *tk_end = '\0';
                  tk_end--;
              }

              if (arr->nelem >= arr->cap) {
                  arr->cap = arr->cap == 0 ? 4 : arr->cap * 2;
                  arr->values = xrealloc(arr->values, arr->cap * sizeof(void*));
              }
              arr->values[arr->nelem++] = strdup(token);
              token = strtok(NULL, ",");
          }

          node_t* n = xmalloc(sizeof(node_t));
          n->type = 3;
          n->key = strdup(k);
          n->value = arr;

          if (cur_tab->node_count >= cur_tab->node_cap) {
              cur_tab->node_cap = cur_tab->node_cap == 0 ? 4 : cur_tab->node_cap * 2;
              cur_tab->nodes = xrealloc(cur_tab->nodes, cur_tab->node_cap * sizeof(node_t*));
          }
          cur_tab->nodes[cur_tab->node_count++] = n;
          continue;
      }
      // Добавление стандартного скалярного значения в текущую таблицу
      node_t *n = xmalloc( sizeof(node_t) );
      n->type = 1;
      n->key = strdup( k );
      n->value = strdup( v );
      if ( cur_tab->node_count >= cur_tab->node_cap )
      {
        cur_tab->node_cap = cur_tab->node_cap == 0 ? 4 : cur_tab->node_cap * 2;
        cur_tab->nodes = xrealloc( cur_tab->nodes,
            cur_tab->node_cap * sizeof(node_t*) );
      }
      cur_tab->nodes[cur_tab->node_count++] = n;
    }
  }
  return root;
}


#include "static/mime.h"

#include <string.h>

typedef struct {
  const char *ext;
  const char *type;
} mime_entry_t;

static const mime_entry_t mime_table[] = {{"html", "text/html; charset=utf-8"},
                                          {"htm", "text/html; charset=utf-8"},
                                          {"css", "text/css; charset=utf-8"},
                                          {"js", "application/javascript"},
                                          {"mjs", "application/javascript"},
                                          {"json", "application/json"},
                                          {"xml", "application/xml"},
                                          {"txt", "text/plain; charset=utf-8"},
                                          {"md", "text/plain; charset=utf-8"},
                                          {"png", "image/png"},
                                          {"jpg", "image/jpeg"},
                                          {"jpeg", "image/jpeg"},
                                          {"gif", "image/gif"},
                                          {"webp", "image/webp"},
                                          {"svg", "image/svg+xml"},
                                          {"ico", "image/x-icon"},
                                          {"wasm", "application/wasm"},
                                          {"pdf", "application/pdf"},
                                          {"zip", "application/zip"},
                                          {"gz", "application/gzip"},
                                          {"mp4", "video/mp4"},
                                          {"webm", "video/webm"},
                                          {"mp3", "audio/mpeg"},
                                          {"ogg", "audio/ogg"},
                                          {"woff", "font/woff"},
                                          {"woff2", "font/woff2"},
                                          {"ttf", "font/ttf"},
                                          {"otf", "font/otf"},
                                          {NULL, NULL}};

const char *mime_by_extension(const char *ext) {
  if (!ext) return "application/octet-stream";
  for (int i = 0; mime_table[i].ext; i++) {
    if (strcasecmp(mime_table[i].ext, ext) == 0) {
      return mime_table[i].type;
    }
  }
  return "application/octet-stream";
}

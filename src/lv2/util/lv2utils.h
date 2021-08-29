static void *get_host_feature(const LV2_Feature *const *host_features, const char *uri) {
  fprintf(stderr, "Finding host feature %s\n", uri);
  for (int i = 0; host_features[i]; i++) {
    if (!strcmp(host_features[i]->URI, uri)) {
      fprintf(stderr, "Found host feature %s\n", uri);
      return host_features[i]->data;
    }
  }
  fprintf(stderr, "Did not find host feature %s\n", uri);
  return NULL;
}

static LV2_URID lv2_map(LV2_URID_Map *map, const char *uri) {
  fprintf(stderr, "Mapping uri %s\n", uri);
  LV2_URID urid = map->map(map->handle, uri);
  fprintf(stderr, "Mapped uri %s to %d\n", uri, urid);
  return urid;
}

static inline const char *lv2_unmap(LV2_URID_Unmap *unmap, LV2_URID urid) {
  fprintf(stderr, "Unmapping urid %d\n", urid);
  const char *uri = unmap->unmap(unmap->handle, urid);
  fprintf(stderr, "Unmapped urid %d to %s\n", urid, uri);
  return uri;
}

static void *get_host_feature(const LV2_Feature *const *host_features, const char *uri) {
  for (int i = 0; host_features[i]; i++) {
    if (!strcmp(host_features[i]->URI, uri)) {
      return host_features[i]->data;
    }
  }
  return NULL;
}

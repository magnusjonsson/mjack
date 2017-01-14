#define FOR(var,limit) for(int var = 0; var < limit; ++var)
#define CHECK(prop, msg) if (!(prop)) { fprintf(stderr, "Error: " msg "\n"); exit(1); }

struct instance {
  char wrapper_cc[128];
  float freq[128];
  float cents[128];
  void *plugin;
  void *wrapper;
};

extern void wrapper_add_cc(struct instance* instance, int cc_number, const char* display_name, const char* persist_name, int default_value);
extern void wrapper_add_audio_input(struct instance* instance, const char* name, float** buf);
extern void wrapper_add_audio_output(struct instance* instance, const char* name, float** buf);
extern void wrapper_add_midi_input(struct instance* instance, const char* name, void** buf);

struct midi_event {
  int time;
  int size;
  const unsigned char *buffer;
};
extern int wrapper_get_num_midi_events(void *buf);
extern struct midi_event wrapper_get_midi_event(void *buf, int i);

extern void plugin_init(struct instance* instance, double sample_rate);
extern void plugin_destroy(struct instance* instance);
extern void plugin_process(struct instance* instance, int nframes);

extern const char* plugin_name;
extern const char* plugin_persistence_name;
extern const unsigned plugin_ladspa_unique_id;

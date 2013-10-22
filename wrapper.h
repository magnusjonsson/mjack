#define FOR(var,limit) for(int var = 0; var < limit; ++var)
#define CHECK(prop, msg) if (!(prop)) { fprintf(stderr, "Error: " msg "\n"); exit(1); }

char wrapper_cc[128];

void wrapper_init(int* argc, char*** argv, const char* title, const char* name);

double wrapper_get_sample_rate(void);

void wrapper_add_cc(int cc_number, const char* display_name, const char* persist_name, int default_value);
void wrapper_add_audio_input(const char* name, float** buf);
void wrapper_add_audio_output(const char* name, float** buf);
void wrapper_add_midi_input(const char* name, void** buf);

void wrapper_run();


void plugin_process(int nframes);

#include "wrapper.h"
#include <getopt.h>
#include <string.h>
#include <memory.h>
#include <jack/jack.h>
#include <jack/session.h>
#include <gtk/gtk.h>
#include <json/json.h>

typedef jack_port_t port_t;
typedef jack_port_t port_t;

static struct instance instance;

static jack_client_t* jack_client;
static jack_status_t jack_status;
#define MAX_NUM_PORTS 8
static int jack_num_ports;
static jack_port_t* jack_port[MAX_NUM_PORTS];
static void** jack_buf[MAX_NUM_PORTS];

#define FOR(var,limit) for(int var = 0; var < limit; ++var)
#define CHECK(prop, msg) if (!(prop)) { fprintf(stderr, "Error: " msg "\n"); exit(1); }

static const char* cc_persist_name[128];

static GtkAdjustment* cc_adjustment[128];

static GtkWidget* window;
static GtkWidget* sliders_box;

static void cb_value_changed(GtkAdjustment* adj, char* pointer_into_cc_array) {
  *pointer_into_cc_array = (int) gtk_adjustment_get_value(adj);
}

static void update_slider(int cc_number) {
  GtkAdjustment* adj = cc_adjustment[cc_number];
  if (adj != NULL) {
    gtk_adjustment_set_value(adj, instance.wrapper_cc[cc_number]);
  }
}

static void save_cc(struct json_object* cc_obj, int cc_number, const char* name) {
  return json_object_object_add(cc_obj, name, json_object_new_int(instance.wrapper_cc[cc_number]));
}

static void save(char* filename) {
  fprintf(stderr, "saving %s\n", filename);
  struct json_object* obj = json_object_new_object();
  json_object_object_add(obj, "info", json_object_new_string("state file for mjack reverb"));
  struct json_object* cc_obj = json_object_new_object();
  json_object_object_add(obj, "cc", cc_obj);
  FOR(i, 128) if (cc_persist_name[i]) save_cc(cc_obj, i, cc_persist_name[i]);
  json_object_to_file(filename, obj);
  json_object_put(obj); // free memory. TODO is this needed?
}

static void load_cc(struct json_object* cc_obj, int cc_number, const char* name) {
  struct json_object* tmp = json_object_object_get(cc_obj, name);
  if (!tmp) {
    fprintf(stderr, "Could not load cc %i (%s)\n", cc_number, name);
    return;
  }
  instance.wrapper_cc[cc_number] = json_object_get_int(tmp);
  update_slider(cc_number);
  fprintf(stderr, "set cc %i to %i\n", cc_number, (int) instance.wrapper_cc[cc_number]);
}

static void load(char* filename) {
  fprintf(stderr, "loading %s\n", filename);
  struct json_object* obj = json_object_from_file(filename);
  if (!obj) goto error;
  struct json_object* cc_obj = json_object_object_get(obj, "cc");
  if (!cc_obj) goto error;
  FOR(i, 128) if (cc_persist_name[i]) load_cc(cc_obj, i, cc_persist_name[i]);
  json_object_put(obj);
  return;
 error:
  fprintf(stderr, "error while loading %s\n", filename);
}

void wrapper_add_cc(struct instance* _instance, int cc_number, const char* display_name, const char* persist_name, int default_value) {
  instance.wrapper_cc[cc_number] = default_value;
  cc_persist_name[cc_number] = persist_name;
  GtkWidget* slider_box = gtk_hbox_new(FALSE, 0);
  GtkObject* adj = gtk_adjustment_new(instance.wrapper_cc[cc_number], 0, 127, 1, 16, 0);
  cc_adjustment[cc_number] = GTK_ADJUSTMENT(adj);
  g_signal_connect(adj, "value_changed", G_CALLBACK(cb_value_changed), &instance.wrapper_cc[cc_number]);
  GtkWidget* label = gtk_label_new(display_name);
  gtk_box_pack_start(GTK_BOX(slider_box), label, FALSE, FALSE, FALSE);
  gtk_widget_show(label);
  GtkWidget* scale = gtk_hscale_new(GTK_ADJUSTMENT(adj));
  gtk_widget_set_size_request(scale, 300, 30);
  gtk_scale_set_digits(GTK_SCALE(scale), 0);
  gtk_box_pack_start(GTK_BOX(slider_box), scale, FALSE, FALSE, FALSE);
  gtk_widget_show(scale);
  gtk_box_pack_start(GTK_BOX(sliders_box), slider_box, FALSE, FALSE, FALSE);
  gtk_widget_show(slider_box);
}

static const char* program_name;
static const char* option_uuid;
static const char* option_dir;

static int gui_session_cb( void *data )
{
  jack_session_event_t *ev = (jack_session_event_t *) data;
  char filename[256];
  char command[256];

  snprintf(filename, sizeof(filename), "%s/state.json", ev->session_dir );
  snprintf(command,  sizeof(command),  "%s --jack-session-uuid=%s \"--jack-session-dir=${SESSION_DIR}\"", program_name, ev->client_uuid);

  save(filename);

  ev->command_line = strdup(command);
  jack_session_reply(jack_client, ev);

  if(ev->type == JackSessionSaveAndQuit)
    gtk_main_quit();

  jack_session_event_free(ev);

  return 0;
}

static void session_cb(jack_session_event_t *event, void *arg)
{
  g_idle_add(gui_session_cb, event);
}

static int process_cb(jack_nframes_t nframes, void* arg) {
  FOR(i, jack_num_ports) *jack_buf[i] = jack_port_get_buffer(jack_port[i], nframes);
  FOR(i, jack_num_ports) if (!jack_buf[i]) return 1;
  plugin_process(&instance, nframes);
  return 0;
}

void wrapper_init(int* argc, char*** argv, const char* title, const char* name) {
  gtk_init(argc, argv);
  program_name = (*argv)[0];
  while (1) {
    int option_index = 0;
    static struct option options[] = {
#define OPTION_JACK_SESSION_UUID 1001
#define OPTION_JACK_SESSION_DIR  1002
      { "jack-session-uuid", required_argument, NULL, OPTION_JACK_SESSION_UUID },
      { "jack-session-dir", required_argument, NULL, OPTION_JACK_SESSION_DIR },
      { NULL, 0, NULL, 0 },
    };
    int c = getopt_long(*argc, *argv, "", options, &option_index);
    if (c == -1) break;
    switch (c) {
    case OPTION_JACK_SESSION_UUID:
      option_uuid = optarg;
      break;
    case OPTION_JACK_SESSION_DIR:
      fprintf(stderr, "option_dir=%s\n", optarg);
      option_dir = optarg;
      break;
    case '?':
      exit(1);
    }
  }
  if (optind < *argc) {
    fprintf(stderr, "unknown argument: %s\n", (*argv)[optind]);
    exit(1);
  }

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect_swapped(G_OBJECT(window), "destroy",
                           G_CALLBACK(gtk_main_quit), NULL);
  gtk_window_set_title(GTK_WINDOW(window), title);
  gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);
  sliders_box = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), sliders_box);

  jack_client = jack_client_open(name, JackSessionID, &jack_status, option_uuid);
  CHECK(jack_client, "jack_client_open");
  CHECK(!jack_status, "jack_client_open");
  CHECK(!jack_set_session_callback(jack_client, session_cb, NULL), "jack_set_session_callback");

  CHECK(!jack_set_process_callback(jack_client, process_cb, NULL), "jack_set_process_callback")
}

void wrapper_run() {
  gtk_widget_show(sliders_box);
  gtk_widget_show(window);
  if (option_dir) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/state.json", option_dir);
    load(filename);
  }
  CHECK(!jack_activate(jack_client), "jack_activate");
  gtk_main();
  jack_deactivate(jack_client);
  FOR(i, jack_num_ports) jack_port_unregister(jack_client, jack_port[i]);
  jack_num_ports = 0;
  jack_client_close(jack_client);
  jack_client = NULL;
}

double wrapper_get_sample_rate(void) {
  return jack_get_sample_rate(jack_client);
}

void wrapper_add_audio_input(struct instance* _instance, const char* name, float** buf) {
  CHECK(jack_num_ports < MAX_NUM_PORTS, "too many ports");
  int i = jack_num_ports++;
  jack_port[i] = jack_port_register(jack_client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  jack_buf[i] = (void**) buf;
}

void wrapper_add_audio_output(struct instance* _instance, const char* name, float** buf) {
  CHECK(jack_num_ports < MAX_NUM_PORTS, "too many ports");
  int i = jack_num_ports++;
  jack_port[i] = jack_port_register(jack_client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  jack_buf[i] = (void**) buf;
}

void wrapper_add_midi_input(struct instance* _instance, const char* name, void** buf) {
  CHECK(jack_num_ports < MAX_NUM_PORTS, "too many ports");
  int i = jack_num_ports++;
  jack_port[i] = jack_port_register(jack_client, name, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  jack_buf[i] = (void**) buf;
}

int main(int argc, char** argv) {
  wrapper_init(&argc, &argv, plugin_name, plugin_persistence_name);
  plugin_init(&instance, wrapper_get_sample_rate());
  wrapper_run();
  plugin_destroy(&instance);
}

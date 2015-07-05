#include <stdio.h>
#include <stdlib.h>
#include <ladspa.h>
#include "wrapper.h"

#define MAX_PORTS 256

struct wrapper {
  int num_ports;
  float* port_cc_value[MAX_PORTS];
  float** port_buf[MAX_PORTS];
};
static int port_cc_number[MAX_PORTS];
static const char* port_names[MAX_PORTS];
static LADSPA_PortDescriptor port_descriptors[MAX_PORTS];
static LADSPA_PortRangeHint port_range_hints[MAX_PORTS];

void wrapper_add_cc(struct instance* instance, int cc_number, const char* display_name, const char* persist_name, int default_value) {
  struct wrapper *w = instance->wrapper;
  CHECK(w->num_ports < MAX_PORTS, "too many ports");
  instance->wrapper_cc[cc_number] = default_value;
  int port = w->num_ports++;
  port_descriptors[port] = LADSPA_PORT_CONTROL | LADSPA_PORT_INPUT;
  port_names[port] = display_name;  
  port_range_hints[port].HintDescriptor = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_MIDDLE;
  port_range_hints[port].LowerBound = 0;
  port_range_hints[port].UpperBound = 127;
  port_cc_number[port] = cc_number;
  w->port_buf[port] = NULL;
};

void wrapper_add_audio_input(struct instance* instance, const char* name, float** buf) {
  struct wrapper *w = instance->wrapper;
  CHECK(w->num_ports < MAX_PORTS, "too many ports");
  int port = w->num_ports++;
  port_descriptors[port] = LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT;
  port_names[port] = name;  
  port_range_hints[port].HintDescriptor = 0;
  port_range_hints[port].LowerBound = 0;
  port_range_hints[port].UpperBound = 0;
  port_cc_number[port] = -1;
  w->port_buf[port] = buf;
}
void wrapper_add_audio_output(struct instance* instance, const char* name, float** buf) {
  struct wrapper *w = instance->wrapper;
  CHECK(w->num_ports < MAX_PORTS, "too many ports");
  int port = w->num_ports++;
  port_descriptors[port] = LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT;
  port_names[port] = name;  
  port_range_hints[port].HintDescriptor = 0;
  port_range_hints[port].LowerBound = 0;
  port_range_hints[port].UpperBound = 0;
  port_cc_number[port] = -1;
  w->port_buf[port] = buf;
}
void wrapper_add_midi_input(struct instance* instance, const char* name, void** buf) {
  *buf = NULL;
  // ignore
}

static void connect_port(LADSPA_Handle Instance,
			 unsigned long Port,
			 LADSPA_Data * DataLocation)
{
  struct instance *instance = Instance;
  struct wrapper *w = instance->wrapper;
  if (port_cc_number[Port] < 0) {
    *(w->port_buf[Port]) = DataLocation;
  } else {
    w->port_cc_value[Port] = DataLocation;
  }
}

static LADSPA_Handle instantiate(const struct _LADSPA_Descriptor * Descriptor,
				 unsigned long                     SampleRate)
{
  struct instance *instance = calloc(1, sizeof(struct instance));
  struct wrapper *w = calloc(1, sizeof(struct wrapper));
  w->num_ports = 0;
  instance->wrapper = w;
  plugin_init(instance, SampleRate);
  return instance;
}

static void activate(LADSPA_Handle Instance) {
}

static void deactivate(LADSPA_Handle Instance) {
}

static void run(LADSPA_Handle Instance,
		unsigned long SampleCount)
{
  struct instance *instance = Instance;
  struct wrapper *w = instance->wrapper;
  CHECK(w->num_ports <= MAX_PORTS, "w->num_ports overflow");
  FOR(i, w->num_ports) {
    if (port_cc_number[i] >= 0) {
      if (!w->port_cc_value[i]) {
	//fprintf(stderr, "Port cc %i not connected\n", i);
	return;
      }
      instance->wrapper_cc[port_cc_number[i]] = *w->port_cc_value[i];
    } else {
      if (!*w->port_buf[i]) {
	//fprintf(stderr, "Port buf %i not connected\n", i);
	return;
      }
    }
  }
  plugin_process(instance, (int) SampleCount);
}

static void cleanup(LADSPA_Handle Instance) {
  struct instance* instance = Instance;
  plugin_destroy(instance);
  free(instance->wrapper);
  instance->wrapper = NULL;
  free(instance);
}

static LADSPA_Descriptor descriptor = {
  .UniqueID = '?',
  .Label = "Nolabel",
  .Name = "Noname",
  .Properties = LADSPA_PROPERTY_REALTIME | LADSPA_PROPERTY_HARD_RT_CAPABLE,
  .Maker = "Magnus Jonsson",
  .Copyright = "2015 Magnus Jonsson",
  .PortCount = 0,
  .PortDescriptors = port_descriptors,
  .PortNames = port_names,
  .PortRangeHints =  port_range_hints,
  .ImplementationData = NULL,
  .instantiate = instantiate,
  .connect_port = connect_port,
  .activate = activate,
  .run = run,
  .run_adding = NULL,
  .set_run_adding_gain = NULL,
  .deactivate = deactivate,
  .cleanup = cleanup,
};

__attribute__ ((visibility ("default")))
const LADSPA_Descriptor *ladspa_descriptor(unsigned long index)  {
  if (index != 0) return NULL;
  struct instance *instance = instantiate(NULL, 0.0);
  descriptor.UniqueID = plugin_ladspa_unique_id;
  descriptor.Label = plugin_name;
  descriptor.Name = plugin_name;
  struct wrapper *w = instance->wrapper;
  descriptor.PortCount = w->num_ports;
  cleanup(instance);
  return &descriptor;
}
